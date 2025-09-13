#pragma once

#include <basalt/wire/text/Component.hpp>

#include <cstdint>
#include <optional>

namespace basalt::wire::multiversion {

/// What to call content a client is too old for and is shown a stand-in of. Wire decides where a
/// name can go; what it says is the server's to write.
class SubstitutionLabels {
public:
    SubstitutionLabels() = default;
    SubstitutionLabels(const SubstitutionLabels&) = default;
    SubstitutionLabels(SubstitutionLabels&&) = default;
    SubstitutionLabels& operator=(const SubstitutionLabels&) = default;
    SubstitutionLabels& operator=(SubstitutionLabels&&) = default;
    virtual ~SubstitutionLabels() = default;

    /// The name for the item `nativeItemId` stands for, or absent to leave the substitute unnamed.
    [[nodiscard]] virtual std::optional<text::Component> itemLabel(std::int32_t nativeItemId) const = 0;

    /// The name to float over an entity of `nativeEntityTypeId`, drawn as some other mob.
    [[nodiscard]] virtual std::optional<text::Component> entityLabel(std::int32_t nativeEntityTypeId) const;
};

} // namespace basalt::wire::multiversion
