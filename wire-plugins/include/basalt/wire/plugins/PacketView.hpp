#pragma once

#include <basalt/wire/lua/LuaArgs.hpp>
#include <basalt/wire/plugins/PluginPackets.hpp>

#include <cstddef>
#include <functional>
#include <memory>

namespace basalt::wire::plugins {

/// What the packet object a listener receives needs from its host.
struct PacketViewHost {
    /// Pushes the player the packet belongs to, or nil.
    std::function<void(lua::LuaArgs&, const PluginPacket&)> pushPlayer;

    /// The most bytes a listener may set as a body or inject with send and receive.
    std::size_t maxBodyBytes = 2 * 1024 * 1024;

    /// The most packets a listener may inject during one delivery.
    std::size_t maxInjected = 64;
};

/// Ties the packet object to the delivery: closing it makes every later use raise instead of touching a dead packet.
class PacketViewLease {
public:
    PacketViewLease(PluginPacket& packet, const PacketViewHost& host) : packet_(&packet), host_(&host) {}

    void close() noexcept { packet_ = nullptr; }

    [[nodiscard]] PluginPacket* packet() const noexcept { return packet_; }

    [[nodiscard]] const PacketViewHost& host() const noexcept { return *host_; }

    [[nodiscard]] bool spend() noexcept { return injected_++ < host_->maxInjected; }

private:
    PluginPacket* packet_;
    const PacketViewHost* host_;
    std::size_t injected_ = 0;
};

/// Pushes the packet table a Lua listener is called with. Close the lease once the call returns.
[[nodiscard]] std::shared_ptr<PacketViewLease> pushPacketView(lua::LuaArgs& lua, PluginPacket& packet,
                                                              const PacketViewHost& host);

} // namespace basalt::wire::plugins
