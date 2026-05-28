#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/ContentRewriter.hpp>
#include <basalt/wire/multiversion/java/JavaTranslators.hpp>
#include <basalt/wire/multiversion/java/Link107.hpp>
#include <basalt/wire/multiversion/java/Link109.hpp>
#include <basalt/wire/multiversion/java/Link110.hpp>
#include <basalt/wire/multiversion/java/Link210.hpp>
#include <basalt/wire/multiversion/java/Link340.hpp>
#include <basalt/wire/multiversion/java/Link404.hpp>
#include <basalt/wire/multiversion/java/Link47.hpp>
#include <basalt/wire/multiversion/java/Link498.hpp>
#include <basalt/wire/multiversion/java/Link578.hpp>
#include <basalt/wire/multiversion/java/Link736.hpp>
#include <basalt/wire/multiversion/java/Link754.hpp>
#include <basalt/wire/multiversion/java/Link755.hpp>
#include <basalt/wire/multiversion/java/Link756.hpp>
#include <basalt/wire/multiversion/java/Link757.hpp>
#include <basalt/wire/multiversion/java/Link758.hpp>
#include <basalt/wire/multiversion/java/Link759.hpp>
#include <basalt/wire/multiversion/java/Link760.hpp>
#include <basalt/wire/multiversion/java/Link761.hpp>
#include <basalt/wire/multiversion/java/Link762.hpp>
#include <basalt/wire/multiversion/java/Link763.hpp>
#include <basalt/wire/multiversion/java/Link764.hpp>
#include <basalt/wire/multiversion/java/Link765.hpp>
#include <basalt/wire/multiversion/java/Link766.hpp>
#include <basalt/wire/multiversion/java/Link767.hpp>
#include <basalt/wire/multiversion/java/Link768.hpp>
#include <basalt/wire/multiversion/java/Link769.hpp>
#include <basalt/wire/multiversion/java/Link770Up.hpp>
#include <basalt/wire/multiversion/java/Link772.hpp>
#include <basalt/wire/multiversion/java/Link772Up.hpp>
#include <basalt/wire/multiversion/java/Link773.hpp>
#include <basalt/wire/multiversion/java/Link773Up.hpp>
#include <basalt/wire/multiversion/java/Link774.hpp>
#include <basalt/wire/multiversion/java/Link774Up.hpp>
#include <basalt/wire/multiversion/java/Link775.hpp>
#include <basalt/wire/multiversion/java/Link775Up.hpp>
#include <basalt/wire/multiversion/java/Link776.hpp>
#include <basalt/wire/multiversion/java/Link776Up.hpp>
#include <basalt/wire/multiversion/java/LoginRewriter.hpp>
#include <basalt/wire/multiversion/java/RenumberingLink.hpp>
#include <basalt/wire/multiversion/java/Tables107.hpp>
#include <basalt/wire/multiversion/java/Tables108.hpp>
#include <basalt/wire/multiversion/java/Tables109.hpp>
#include <basalt/wire/multiversion/java/Tables110.hpp>
#include <basalt/wire/multiversion/java/Tables210.hpp>
#include <basalt/wire/multiversion/java/Tables315.hpp>
#include <basalt/wire/multiversion/java/Tables316.hpp>
#include <basalt/wire/multiversion/java/Tables335.hpp>
#include <basalt/wire/multiversion/java/Tables338.hpp>
#include <basalt/wire/multiversion/java/Tables340.hpp>
#include <basalt/wire/multiversion/java/Tables393.hpp>
#include <basalt/wire/multiversion/java/Tables401.hpp>
#include <basalt/wire/multiversion/java/Tables404.hpp>
#include <basalt/wire/multiversion/java/Tables47.hpp>
#include <basalt/wire/multiversion/java/Tables477.hpp>
#include <basalt/wire/multiversion/java/Tables480.hpp>
#include <basalt/wire/multiversion/java/Tables485.hpp>
#include <basalt/wire/multiversion/java/Tables490.hpp>
#include <basalt/wire/multiversion/java/Tables498.hpp>
#include <basalt/wire/multiversion/java/Tables573.hpp>
#include <basalt/wire/multiversion/java/Tables575.hpp>
#include <basalt/wire/multiversion/java/Tables578.hpp>
#include <basalt/wire/multiversion/java/Tables735.hpp>
#include <basalt/wire/multiversion/java/Tables736.hpp>
#include <basalt/wire/multiversion/java/Tables751.hpp>
#include <basalt/wire/multiversion/java/Tables753.hpp>
#include <basalt/wire/multiversion/java/Tables754.hpp>
#include <basalt/wire/multiversion/java/Tables755.hpp>
#include <basalt/wire/multiversion/java/Tables756.hpp>
#include <basalt/wire/multiversion/java/Tables757.hpp>
#include <basalt/wire/multiversion/java/Tables758.hpp>
#include <basalt/wire/multiversion/java/Tables759.hpp>
#include <basalt/wire/multiversion/java/Tables760.hpp>
#include <basalt/wire/multiversion/java/Tables761.hpp>
#include <basalt/wire/multiversion/java/Tables762.hpp>
#include <basalt/wire/multiversion/java/Tables763.hpp>
#include <basalt/wire/multiversion/java/Tables764.hpp>
#include <basalt/wire/multiversion/java/Tables765.hpp>
#include <basalt/wire/multiversion/java/Tables766.hpp>
#include <basalt/wire/multiversion/java/Tables767.hpp>
#include <basalt/wire/multiversion/java/Tables768.hpp>
#include <basalt/wire/multiversion/java/Tables769.hpp>
#include <basalt/wire/multiversion/java/Tables770.hpp>
#include <basalt/wire/multiversion/java/Tables771.hpp>
#include <basalt/wire/multiversion/java/Tables772.hpp>
#include <basalt/wire/multiversion/java/Tables773.hpp>
#include <basalt/wire/multiversion/java/Tables774.hpp>
#include <basalt/wire/multiversion/java/Tables775.hpp>
#include <basalt/wire/multiversion/java/Tables776.hpp>
#include <basalt/wire/multiversion/java/UpEntityData.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

struct LinkEntry {
    std::int32_t version;
    const VersionTables& (*tables)();
    std::unique_ptr<ProtocolLink> (*make)(const VersionTables&, const ComposedTables&);
};

template <typename T>
std::unique_ptr<ProtocolLink> plain(const VersionTables& tables, const ComposedTables&) {
    return std::make_unique<T>(tables);
}

template <typename T>
std::unique_ptr<ProtocolLink> composed(const VersionTables& tables, const ComposedTables& target) {
    return std::make_unique<T>(tables, target);
}

/// Newest first.
constexpr std::array<LinkEntry, 49> kLinks = {{
    {protocolVersions::kV26_2, &tables776, &composed<Link776>},
    {protocolVersions::kV26_1, &tables775, &plain<Link775>},
    {protocolVersions::kV1_21_11, &tables774, &plain<Link774>},
    {protocolVersions::kV1_21_9, &tables773, &plain<Link773>},
    {protocolVersions::kV1_21_7, &tables772, &plain<Link772>},
    {protocolVersions::kV1_21_6, &tables771, &plain<RenumberingLink>},
    {protocolVersions::kV1_21_5, &tables770, &plain<RenumberingLink>},
    {protocolVersions::kV1_21_4, &tables769, &plain<Link769>},
    {protocolVersions::kV1_21_2, &tables768, &plain<Link768>},
    {protocolVersions::kV1_21, &tables767, &plain<Link767>},
    {protocolVersions::kV1_20_5, &tables766, &plain<Link766>},
    {protocolVersions::kV1_20_3, &tables765, &composed<Link765>},
    {protocolVersions::kV1_20_2, &tables764, &plain<Link764>},
    {protocolVersions::kV1_20, &tables763, &plain<Link763>},
    {protocolVersions::kV1_19_4, &tables762, &plain<Link762>},
    {protocolVersions::kV1_19_3, &tables761, &composed<Link761>},
    {protocolVersions::kV1_19_1, &tables760, &plain<Link760>},
    {protocolVersions::kV1_19, &tables759, &plain<Link759>},
    {protocolVersions::kV1_18_2, &tables758, &plain<Link758>},
    {protocolVersions::kV1_18, &tables757, &plain<Link757>},
    {protocolVersions::kV1_17_1, &tables756, &plain<Link756>},
    {protocolVersions::kV1_17, &tables755, &plain<Link755>},
    {protocolVersions::kV1_16_4, &tables754, &plain<Link754>},
    {protocolVersions::kV1_16_3, &tables753, &plain<RenumberingLink>},
    {protocolVersions::kV1_16_2, &tables751, &plain<RenumberingLink>},
    {protocolVersions::kV1_16_1, &tables736, &composed<Link736>},
    {protocolVersions::kV1_16, &tables735, &plain<RenumberingLink>},
    {protocolVersions::kV1_15_2, &tables578, &plain<Link578>},
    {protocolVersions::kV1_15_1, &tables575, &plain<RenumberingLink>},
    {protocolVersions::kV1_15, &tables573, &plain<RenumberingLink>},
    {protocolVersions::kV1_14_4, &tables498, &plain<Link498>},
    {protocolVersions::kV1_14_3, &tables490, &plain<RenumberingLink>},
    {protocolVersions::kV1_14_2, &tables485, &plain<RenumberingLink>},
    {protocolVersions::kV1_14_1, &tables480, &plain<RenumberingLink>},
    {protocolVersions::kV1_14, &tables477, &plain<RenumberingLink>},
    {protocolVersions::kV1_13_2, &tables404, &plain<Link404>},
    {protocolVersions::kV1_13_1, &tables401, &plain<RenumberingLink>},
    {protocolVersions::kV1_13, &tables393, &plain<RenumberingLink>},
    {protocolVersions::kV1_12_2, &tables340, &plain<Link340>},
    {protocolVersions::kV1_12_1, &tables338, &plain<RenumberingLink>},
    {protocolVersions::kV1_12, &tables335, &plain<RenumberingLink>},
    {protocolVersions::kV1_11_1, &tables316, &plain<RenumberingLink>},
    {protocolVersions::kV1_11, &tables315, &plain<RenumberingLink>},
    {protocolVersions::kV1_10, &tables210, &plain<Link210>},
    {protocolVersions::kV1_9_4, &tables110, &plain<Link110>},
    {protocolVersions::kV1_9_2, &tables109, &plain<Link109>},
    {protocolVersions::kV1_9_1, &tables108, &plain<RenumberingLink>},
    {protocolVersions::kV1_9, &tables107, &plain<Link107>},
    {protocolVersions::kV1_8, &tables47, &plain<Link47>},
}};

class Chain final : public JavaChain {
public:
    Chain(std::span<const LinkEntry> used, const SubstitutionLabels* labels, std::int32_t protocol) {
        std::vector<std::reference_wrapper<const VersionTables>> tables;
        std::vector<std::int32_t> versions;
        tables.reserve(used.size());
        versions.reserve(used.size());
        for (const LinkEntry& entry : used) {
            tables.emplace_back(entry.tables());
            versions.push_back(entry.version);
        }
        composed_ = std::make_unique<ComposedTables>(tables);
        content_ = std::make_unique<ContentRewriter>(*composed_, labels, protocol, std::move(versions));
        login_ = std::make_unique<LoginRewriter>(protocol);
        links_.reserve(used.size());
        std::vector<std::reference_wrapper<ProtocolLink>> refs;
        refs.reserve(used.size());
        for (const LinkEntry& entry : used) {
            links_.push_back(entry.make(entry.tables(), *composed_));
            refs.emplace_back(*links_.back());
        }
        chain_ = std::make_unique<ProtocolChain>(std::move(refs), content_.get(), login_.get());
    }

    [[nodiscard]] const ProtocolChain& chain() const override { return *chain_; }

private:
    std::unique_ptr<ComposedTables> composed_;
    std::unique_ptr<ContentRewriter> content_;
    std::unique_ptr<LoginRewriter> login_;
    std::vector<std::unique_ptr<ProtocolLink>> links_;
    std::unique_ptr<ProtocolChain> chain_;
};

struct UpEntry {
    std::int32_t version;
    const VersionTables& (*tables)();
    std::unique_ptr<ProtocolLink> (*make)(const VersionTables& upward);
};

template <typename T>
std::unique_ptr<ProtocolLink> climbing(const VersionTables& upward) {
    return std::make_unique<T>(upward);
}

constexpr std::array<UpEntry, 7> kUpLinks = {{
    {protocolVersions::kV26_2, &tables776, &climbing<Link776Up>},
    {protocolVersions::kV26_1, &tables775, &climbing<Link775Up>},
    {protocolVersions::kV1_21_11, &tables774, &climbing<Link774Up>},
    {protocolVersions::kV1_21_9, &tables773, &climbing<Link773Up>},
    {protocolVersions::kV1_21_7, &tables772, &climbing<Link772Up>},
    {protocolVersions::kV1_21_6, &tables771, &climbing<RenumberingLink>},
    {protocolVersions::kV1_21_5, &tables770, &climbing<Link770Up>},
}};

class UpChain final : public JavaChain {
public:
    UpChain(std::span<const UpEntry> used, std::int32_t backendVersion, std::int32_t clientVersion) {
        upward_.reserve(used.size());
        std::int32_t newer = clientVersion;
        for (const UpEntry& entry : used) {
            upward_.push_back(invertedTables(entry.tables(), newer));
            newer = entry.version;
        }
        std::ranges::reverse(upward_);
        std::vector<std::reference_wrapper<const VersionTables>> tables(upward_.begin(), upward_.end());
        composed_ = std::make_unique<ComposedTables>(tables);
        content_ =
            std::make_unique<ContentRewriter>(*composed_, nullptr, protocolVersions::kNative,
                                              std::vector<std::int32_t>{}, backendVersion >= protocolVersions::kV26_1);
        tail_ = std::make_unique<UpEntityData>(*content_, *composed_, backendVersion);
        std::vector<std::reference_wrapper<ProtocolLink>> refs;
        for (std::size_t index = 0; index < used.size(); ++index) {
            links_.push_back(used[used.size() - 1 - index].make(upward_[index]));
            refs.emplace_back(*links_.back());
        }
        chain_ = std::make_unique<ProtocolChain>(std::move(refs), nullptr, tail_.get(), true);
    }

    [[nodiscard]] const ProtocolChain& chain() const override { return *chain_; }

private:
    std::vector<VersionTables> upward_;
    std::unique_ptr<ComposedTables> composed_;
    std::unique_ptr<ContentRewriter> content_;
    std::unique_ptr<UpEntityData> tail_;
    std::vector<std::unique_ptr<ProtocolLink>> links_;
    std::unique_ptr<ProtocolChain> chain_;
};

} // namespace

namespace javaTranslators {

std::int32_t packetIdAt(std::int32_t version, protocol::ProtocolState state, bool clientbound, std::int32_t nativeId) {
    std::int32_t id = nativeId;
    if (version == protocolVersions::kNative) {
        return id;
    }
    for (const LinkEntry& entry : kLinks) {
        const PacketIdTables& packets = entry.tables().packets;
        id = clientbound ? packets.toOlder(state, id) : packets.serverboundToOlder(state, id);
        if (id < 0 || entry.version == version) {
            return id;
        }
    }
    return -1;
}

const std::set<std::int32_t>& upSupported() {
    static const std::set<std::int32_t> versions = [] {
        std::set<std::int32_t> reachable;
        for (const UpEntry& entry : kUpLinks) {
            reachable.insert(entry.version);
        }
        return reachable;
    }();
    return versions;
}

std::unique_ptr<JavaChain> upChainFor(std::int32_t backendProtocol, std::int32_t clientProtocol) {
    std::size_t first = 0;
    while (first < kUpLinks.size() && kUpLinks[first].version >= clientProtocol) {
        first++;
    }
    std::size_t used = first;
    while (used < kUpLinks.size() && kUpLinks[used].version >= backendProtocol) {
        used++;
    }
    if (used == first || kUpLinks[used - 1].version != backendProtocol) {
        return nullptr;
    }
    return std::make_unique<UpChain>(std::span{kUpLinks}.subspan(first, used - first), backendProtocol, clientProtocol);
}

const std::set<std::int32_t>& supported() {
    static const std::set<std::int32_t> versions = [] {
        std::set<std::int32_t> reachable{protocolVersions::kNative};
        std::int32_t expected = protocolVersions::kNative;
        for (const LinkEntry& entry : kLinks) {
            // The next one down has to be exactly the version below: a gap in the road is no road.
            const std::span<const ProtocolVersion> known = protocolVersions::known();
            std::int32_t below = 0;
            for (const ProtocolVersion& one : known) {
                if (one.number < expected) {
                    below = one.number;
                }
            }
            if (below == 0 || entry.version != below) {
                break;
            }
            reachable.insert(entry.version);
            expected = entry.version;
        }
        return reachable;
    }();
    return versions;
}

std::unique_ptr<JavaChain> chainFor(std::int32_t protocol, const SubstitutionLabels* labels) {
    if (protocol == protocolVersions::kNative || !supported().contains(protocol)) {
        return nullptr;
    }
    std::size_t used = 0;
    while (used < kLinks.size() && kLinks[used].version >= protocol) {
        used++;
    }
    return std::make_unique<Chain>(std::span{kLinks}.first(used), labels, protocol);
}

std::int32_t blockStateAs(std::int32_t protocol, std::int32_t nativeState) {
    if (protocol == protocolVersions::kNative) {
        return nativeState;
    }
    std::int32_t state = nativeState;
    for (const LinkEntry& entry : kLinks) {
        state = entry.tables().blockStates.map(state);
        if (entry.version == protocol || state == IdMap::kNone) {
            return state;
        }
    }
    return nativeState;
}

} // namespace javaTranslators

} // namespace basalt::wire::multiversion::java
