#include <basalt/wire/multiversion/SubstitutionLabels.hpp>

#include <cstdint>

namespace basalt::wire::multiversion {

std::optional<text::Component> SubstitutionLabels::entityLabel(std::int32_t) const { return std::nullopt; }

} // namespace basalt::wire::multiversion
