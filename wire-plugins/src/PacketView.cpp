#include <basalt/wire/lua/LuaCall.hpp>
#include <basalt/wire/plugins/PacketNaming.hpp>
#include <basalt/wire/plugins/PacketView.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::plugins {
namespace {
using lua::LuaArgs;

[[nodiscard]] std::vector<std::uint8_t> bytesFrom(LuaArgs& args, std::int32_t index, std::size_t limit) {
    if (!args.isTable(index)) {
        args.raise("packet bytes are a list of numbers from 0 to 255");
    }
    const std::int64_t length = args.length(index);
    if (length < 0 || static_cast<std::size_t>(length) > limit) {
        args.raise("packet bytes are limited to " + std::to_string(limit) + " bytes here");
    }
    std::vector<std::uint8_t> out;
    out.reserve(static_cast<std::size_t>(length));
    for (std::int64_t position = 1; position <= length; position++) {
        args.pushIndex(index, position);
        out.push_back(static_cast<std::uint8_t>(args.toInteger(-1) & 0xFF));
        args.pop(1);
    }
    return out;
}

[[nodiscard]] std::string_view phaseName(PacketPhase phase) {
    switch (phase) {
    case PacketPhase::Handshake:
        return "handshake";
    case PacketPhase::Status:
        return "status";
    case PacketPhase::Login:
        return "login";
    case PacketPhase::Configuration:
        return "configuration";
    case PacketPhase::Play:
        return "play";
    }
    return "play";
}

[[nodiscard]] PluginPacket& live(LuaArgs& args, const PacketViewLease& lease) {
    PluginPacket* packet = lease.packet();
    if (packet == nullptr) {
        args.raise("this packet is only valid while its listener runs");
    }
    return *packet;
}

[[nodiscard]] std::int32_t firstArgument(const LuaArgs& args) { return args.count() >= 1 && args.isTable(1) ? 2 : 1; }

// Sends or injects another packet on the connection this one travels on, past the listeners unless asked.
[[nodiscard]] bool relay(LuaArgs& args, PacketViewLease& lease, bool inbound) {
    PluginPacket& from = live(args, lease);
    const std::int32_t base = firstArgument(args);
    const std::string name = args.checkString(base);
    const std::optional<PacketAddress> address = packetNaming::resolve(name);
    if (!address.has_value() || address->serverbound != inbound) {
        args.raise("'" + name + "' is not a " + (inbound ? "serverbound" : "clientbound") + " packet");
    }
    if (address->phase != from.address.phase) {
        args.raise("'" + name + "' belongs to another protocol phase than the connection is in");
    }
    const std::vector<std::uint8_t> body = bytesFrom(args, base + 1, lease.host().maxBodyBytes);
    if (!lease.spend()) {
        args.raise("a listener may send at most " + std::to_string(lease.host().maxInjected) + " packets per call");
    }
    if (from.endpoint == nullptr) {
        return false;
    }
    const bool intercept = args.isBoolean(base + 2) && args.booleanOr(base + 2, false);
    return inbound ? from.endpoint->receiveServerbound(address->id, body, intercept)
                   : from.endpoint->sendClientbound(address->id, body, intercept);
}

void pushMethod(LuaArgs& args, const std::shared_ptr<PacketViewLease>& lease, std::string_view name) {
    if (name == "cancel") {
        lua::pushHostFunction(args, "packet.cancel", [lease](LuaArgs& inner) {
            PluginPacket& packet = live(inner, *lease);
            if (packet.editable) {
                packet.cancelled = true;
            }
            return 0;
        });
    } else if (name == "send" || name == "receive") {
        const bool inbound = name == "receive";
        lua::pushHostFunction(args, inbound ? "packet.receive" : "packet.send", [lease, inbound](LuaArgs& inner) {
            inner.pushBoolean(relay(inner, *lease, inbound));
            return 1;
        });
    } else {
        args.pushNil();
    }
}

void pushField(LuaArgs& args, const std::shared_ptr<PacketViewLease>& lease, std::string_view key) {
    PluginPacket& packet = live(args, *lease);
    if (key == "name") {
        args.pushString(packet.name);
    } else if (key == "serverbound") {
        args.pushBoolean(packet.address.serverbound);
    } else if (key == "phase") {
        args.pushString(phaseName(packet.address.phase));
    } else if (key == "id") {
        args.pushInteger(packet.address.id);
    } else if (key == "connection") {
        args.pushInteger(static_cast<std::int64_t>(packet.connection));
    } else if (key == "protocol") {
        args.pushInteger(packet.clientProtocol);
    } else if (key == "backend_protocol") {
        args.pushInteger(packet.backendProtocol);
    } else if (key == "translated") {
        args.pushBoolean(packet.translated);
    } else if (key == "size") {
        args.pushInteger(static_cast<std::int64_t>(packet.bytes.size()));
    } else if (key == "player_name") {
        args.pushString(packet.playerName);
    } else if (key == "player_uuid") {
        args.pushString(packet.playerUuid);
    } else if (key == "backend") {
        args.pushString(packet.backend);
    } else if (key == "cancelled") {
        args.pushBoolean(packet.cancelled);
    } else if (key == "editable") {
        args.pushBoolean(packet.editable);
    } else if (key == "player") {
        if (lease->host().pushPlayer) {
            lease->host().pushPlayer(args, packet);
        } else {
            args.pushNil();
        }
    } else if (key == "bytes") {
        args.pushTable();
        std::int64_t position = 1;
        for (const std::uint8_t byte : packet.bytes) {
            args.pushInteger(byte);
            args.setIndex(position++);
        }
    } else {
        pushMethod(args, lease, key);
    }
}
} // namespace

std::shared_ptr<PacketViewLease> pushPacketView(LuaArgs& lua, PluginPacket& packet, const PacketViewHost& host) {
    auto lease = std::make_shared<PacketViewLease>(packet, host);
    lua.pushTable();
    lua.pushTable();
    lua::pushHostFunction(lua, "packet.index", [lease](LuaArgs& args) {
        pushField(args, lease, args.checkString(2));
        return 1;
    });
    lua.setField("__index");
    lua::pushHostFunction(lua, "packet.newindex", [lease](LuaArgs& args) {
        PluginPacket& target = live(args, *lease);
        const std::string key = args.checkString(2);
        if (key == "cancelled") {
            if (target.editable) {
                target.cancelled = args.booleanOr(3, true);
            }
        } else if (key == "bytes") {
            std::vector<std::uint8_t> body = bytesFrom(args, 3, lease->host().maxBodyBytes);
            if (target.editable) {
                target.replacement = std::move(body);
            }
        } else {
            args.raise("a packet has no writable field '" + key + "'; cancelled and bytes can be set");
        }
        return 0;
    });
    lua.setField("__newindex");
    lua.setMetatable(-2);
    return lease;
}
} // namespace basalt::wire::plugins
