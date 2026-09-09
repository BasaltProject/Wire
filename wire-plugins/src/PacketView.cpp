#include <basalt/wire/lua/LuaCall.hpp>
#include <basalt/wire/plugins/PacketNaming.hpp>
#include <basalt/wire/plugins/PacketView.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::wire::plugins {
namespace {
using lua::LuaArgs;

using Bytes = std::expected<std::vector<std::uint8_t>, std::string>;

[[nodiscard]] Bytes bytesFrom(LuaArgs& args, std::int32_t index, std::size_t limit) {
    if (!args.isTable(index)) {
        return std::unexpected{"packet bytes are a list of numbers from 0 to 255"};
    }
    const std::int64_t length = args.length(index);
    if (length < 0 || static_cast<std::size_t>(length) > limit) {
        return std::unexpected{"packet bytes are limited to " + std::to_string(limit) + " bytes here"};
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

constexpr std::string_view kExpired = "this packet is only valid while its listener runs";

[[nodiscard]] std::int32_t firstArgument(const LuaArgs& args) { return args.count() >= 1 && args.isTable(1) ? 2 : 1; }

// Sends or injects another packet on the connection this one travels on, past the listeners unless asked.
[[nodiscard]] std::expected<bool, std::string> relay(LuaArgs& args, PacketViewLease& lease, bool inbound) {
    PluginPacket* const live = lease.packet();
    if (live == nullptr) {
        return std::unexpected{std::string{kExpired}};
    }
    PluginPacket& from = *live;
    const std::int32_t base = firstArgument(args);
    const std::string name = args.checkString(base);
    const std::optional<PacketAddress> address = packetNaming::resolve(name);
    if (!address.has_value() || address->serverbound != inbound) {
        return std::unexpected{"'" + name + "' is not a " + (inbound ? "serverbound" : "clientbound") + " packet"};
    }
    if (address->phase != from.address.phase) {
        return std::unexpected{"'" + name + "' belongs to another protocol phase than the connection is in"};
    }
    const Bytes body = bytesFrom(args, base + 1, lease.host().maxBodyBytes);
    if (!body) {
        return std::unexpected{body.error()};
    }
    if (!lease.spend()) {
        return std::unexpected{"a listener may send at most " + std::to_string(lease.host().maxInjected) +
                               " packets per call"};
    }
    if (from.endpoint == nullptr) {
        return false;
    }
    const bool intercept = args.isBoolean(base + 2) && args.booleanOr(base + 2, false);
    return inbound ? from.endpoint->receiveServerbound(address->id, *body, intercept)
                   : from.endpoint->sendClientbound(address->id, *body, intercept);
}

void pushMethod(LuaArgs& args, const std::shared_ptr<PacketViewLease>& lease, std::string_view name) {
    if (name == "cancel") {
        lua::pushHostFunction(args, "packet.cancel", [lease](LuaArgs& inner) {
            PluginPacket* const packet = lease->packet();
            if (packet == nullptr) {
                return inner.fail(kExpired);
            }
            if (packet->editable) {
                packet->cancelled = true;
            }
            return 0;
        });
    } else if (name == "send" || name == "receive") {
        const bool inbound = name == "receive";
        lua::pushHostFunction(args, inbound ? "packet.receive" : "packet.send", [lease, inbound](LuaArgs& inner) {
            const auto sent = relay(inner, *lease, inbound);
            if (!sent) {
                return inner.fail(sent.error());
            }
            inner.pushBoolean(*sent);
            return 1;
        });
    } else {
        args.pushNil();
    }
}

[[nodiscard]] bool pushField(LuaArgs& args, const std::shared_ptr<PacketViewLease>& lease, std::string_view key) {
    PluginPacket* const live = lease->packet();
    if (live == nullptr) {
        return false;
    }
    PluginPacket& packet = *live;
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
    return true;
}
} // namespace

std::shared_ptr<PacketViewLease> pushPacketView(LuaArgs& lua, PluginPacket& packet, const PacketViewHost& host) {
    auto lease = std::make_shared<PacketViewLease>(packet, host);
    lua.pushTable();
    lua.pushTable();
    lua::pushHostFunction(lua, "packet.index", [lease](LuaArgs& args) {
        const std::string key = args.checkString(2);
        return pushField(args, lease, key) ? 1 : args.fail(kExpired);
    });
    lua.setField("__index");
    lua::pushHostFunction(lua, "packet.newindex", [lease](LuaArgs& args) {
        PluginPacket* const target = lease->packet();
        if (target == nullptr) {
            return args.fail(kExpired);
        }
        const std::string key = args.checkString(2);
        if (key == "cancelled") {
            if (target->editable) {
                target->cancelled = args.booleanOr(3, true);
            }
        } else if (key == "bytes") {
            Bytes body = bytesFrom(args, 3, lease->host().maxBodyBytes);
            if (!body) {
                return args.fail(body.error());
            }
            if (target->editable) {
                target->replacement = std::move(*body);
            }
        } else {
            return args.fail("a packet has no writable field '" + key + "'; cancelled and bytes can be set");
        }
        return 0;
    });
    lua.setField("__newindex");
    lua.setMetatable(-2);
    return lease;
}
} // namespace basalt::wire::plugins
