#include <basalt/wire/multiversion/java/CommandParsers.hpp>
#include <basalt/wire/multiversion/java/Link761Up.hpp>
#include <basalt/wire/multiversion/java/PlayerSpawn.hpp>

namespace basalt::wire::multiversion::java {
namespace {

constexpr std::int32_t kNamedEntitySpawn = 2;
constexpr std::int32_t kPosition = 56;
constexpr std::int32_t kSpawnEntity762 = 1;
constexpr std::int32_t kCommands = 14;
constexpr std::int32_t kHeightmapParser = 47;

std::int32_t parserIn762(std::int32_t parser) { return parser >= kHeightmapParser ? parser + 1 : parser; }

} // namespace

bool Link761Up::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Play) {
        return true;
    }
    if (packet.id == kNamedEntitySpawn) {
        rewrite(packet,
                [&](io::ByteBuffer& in, io::ByteBuffer& out) { playerAsEntity(in, out, tables().playerEntityType); });
        packet.overrideId(kSpawnEntity762);
    } else if (packet.id == kPosition && !packet.body.empty()) {
        packet.body.pop_back();
    } else if (packet.id == kCommands) {
        commandParsers::Layout layout;
        layout.scoreHolder = 29;
        layout.time = 40;
        layout.firstRegistry = 41;
        layout.lastRegistry = 44;
        layout.timeAddsMinimum = true;
        static_cast<void>(commandParsers::remap(packet, layout, parserIn762));
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
