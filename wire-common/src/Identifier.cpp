#include <basalt/wire/common/Identifier.hpp>

namespace basalt::wire::common {

std::expected<Identifier, IdentifierError> Identifier::parse(std::string_view value) {
    const std::size_t separator = value.find(':');
    if (separator == std::string_view::npos) {
        return Identifier{std::string{kVanillaNamespace}, std::string{value}};
    }
    if (separator != value.rfind(':')) {
        return std::unexpected{IdentifierError::TooManySeparators};
    }

    const std::string_view namespaceName = value.substr(0, separator);
    const std::string_view path = value.substr(separator + 1);
    if (namespaceName.empty() || path.empty()) {
        return std::unexpected{IdentifierError::EmptyPart};
    }
    return Identifier{std::string{namespaceName}, std::string{path}};
}

} // namespace basalt::wire::common
