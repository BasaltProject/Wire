#pragma once

#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <optional>

namespace basalt::wire::multiversion {

class SubstitutionLabels {
public:
    SubstitutionLabels() = default;
    SubstitutionLabels(const SubstitutionLabels&) = default;
    SubstitutionLabels(SubstitutionLabels&&) = default;
    SubstitutionLabels& operator=(const SubstitutionLabels&) = default;
    SubstitutionLabels& operator=(SubstitutionLabels&&) = default;
    virtual ~SubstitutionLabels() = default;

    [[nodiscard]] virtual std::optional<text::Component> itemLabel(std::int32_t nativeItemId) const = 0;

    [[nodiscard]] virtual std::optional<text::Component> entityLabel(std::int32_t nativeEntityTypeId) const;
};

} // namespace basalt::wire::multiversion
