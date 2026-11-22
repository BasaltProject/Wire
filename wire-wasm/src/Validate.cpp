#include "Internal.hpp"

#include <basalt/wire/wasm/Abi.hpp>

namespace basalt::wire::wasm::detail {
namespace {
[[nodiscard]] const ExportInfo* findExport(const ModuleInfo& info, std::string_view name) {
    for (const ExportInfo& entry : info.exports) {
        if (entry.name == name) {
            return &entry;
        }
    }
    return nullptr;
}

[[nodiscard]] Result<void> requireFunction(const ModuleInfo& info, std::string_view name, std::string_view signature) {
    const ExportInfo* entry = findExport(info, name);
    if (entry == nullptr) {
        return fail(ErrorCode::InvalidModule, "the module does not export " + std::string{name});
    }
    if (entry->kind != 'f' || entry->signature != signature) {
        return fail(ErrorCode::InvalidModule, std::string{name} + " must be a function with signature " +
                                                  std::string{signature} + ", found " + entry->signature);
    }
    return {};
}
} // namespace

Result<void> validateModule(const ModuleInfo& info, Profile profile) {
    for (const ImportInfo& entry : info.imports) {
        const std::string label = entry.module + "." + entry.name;
        if (entry.module != kBasaltModule && entry.module != kWasiModule && entry.module != kProxyModule) {
            return fail(ErrorCode::InvalidModule, "import " + label + " comes from a module the host does not offer");
        }
        const std::optional<std::size_t> index = findAbiFunction(entry.module, entry.name);
        if (!index) {
            return fail(ErrorCode::InvalidModule, "import " + label + " is not part of the plugin ABI");
        }
        if (!inProfile(profile, abiFunctions()[*index])) {
            return fail(ErrorCode::InvalidModule, "import " + label + " is not part of the " +
                                                      (profile == Profile::Proxy ? "proxy" : "server") + " profile");
        }
        if (entry.kind != 'f') {
            return fail(ErrorCode::InvalidModule, "import " + label + " must be a function");
        }
        const std::string_view expected = abiFunctions()[*index].signature;
        if (entry.signature != expected) {
            return fail(ErrorCode::InvalidModule, "import " + label + " has signature " + entry.signature +
                                                      ", the ABI says " + std::string{expected});
        }
    }
    const ExportInfo* memory = findExport(info, "memory");
    if (memory == nullptr || memory->kind != 'm') {
        return fail(ErrorCode::InvalidModule, "the module does not export its memory as 'memory'");
    }
    if (findExport(info, "_initialize") != nullptr) {
        if (auto ok = requireFunction(info, "_initialize", ">"); !ok) {
            return ok;
        }
    }
    if (auto ok = requireFunction(info, "basalt_abi_version", ">i"); !ok) {
        return ok;
    }
    if (auto ok = requireFunction(info, "basalt_init", ">"); !ok) {
        return ok;
    }
    return requireFunction(info, "basalt_callback", "ii>");
}

} // namespace basalt::wire::wasm::detail
