#include "Internal.hpp"

#include <basalt/wire/wasm/Abi.hpp>

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace basalt::wire::wasm {
namespace {
constexpr std::array kWasi = std::to_array<AbiFunction>({
    {kWasiModule, "fd_write", "iiii>i", AbiScope::Shared},
    {kWasiModule, "clock_time_get", "ili>i", AbiScope::Shared},
    {kWasiModule, "random_get", "ii>i", AbiScope::Shared},
    {kWasiModule, "environ_sizes_get", "ii>i", AbiScope::Shared},
    {kWasiModule, "environ_get", "ii>i", AbiScope::Shared},
    {kWasiModule, "args_sizes_get", "ii>i", AbiScope::Shared},
    {kWasiModule, "args_get", "ii>i", AbiScope::Shared},
    {kWasiModule, "proc_exit", "i>", AbiScope::Shared},
});

struct Table {
    std::vector<AbiFunction> functions;
    std::unordered_map<std::string, std::size_t> index;

    Table() {
        const std::span<const AbiFunction> generated = detail::generatedAbiFunctions();
        functions.assign(generated.begin(), generated.end());
        functions.insert(functions.end(), kWasi.begin(), kWasi.end());
        for (std::size_t position = 0; position < functions.size(); ++position) {
            index.emplace(std::string{functions[position].module} + "." + std::string{functions[position].name},
                          position);
        }
    }
};

const Table& table() {
    static const Table instance;
    return instance;
}
} // namespace

std::span<const AbiFunction> abiFunctions() { return table().functions; }

bool inProfile(Profile profile, const AbiFunction& function) {
    switch (function.scope) {
    case AbiScope::Shared:
        return true;
    case AbiScope::Proxy:
        return profile == Profile::Proxy;
    case AbiScope::Server:
        return profile == Profile::Server;
    }
    return false;
}

std::size_t basaltFunctionCount() {
    std::size_t count = 0;
    for (const AbiFunction& function : table().functions) {
        count += function.module == kBasaltModule ? 1U : 0U;
    }
    return count;
}

std::optional<std::size_t> findAbiFunction(std::string_view module, std::string_view name) {
    const Table& instance = table();
    const auto found = instance.index.find(std::string{module} + "." + std::string{name});
    return found == instance.index.end() ? std::nullopt : std::optional<std::size_t>{found->second};
}

} // namespace basalt::wire::wasm
