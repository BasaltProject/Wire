#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace basalt::wire::plugins {

/// Removes what it registered when `remove` is called; calling it again does nothing.
class PluginHandle {
public:
    using Remove = std::function<void()>;

    PluginHandle() = default;

    explicit PluginHandle(Remove remove) : remove_(std::move(remove)) {}

    void remove() {
        if (remove_) {
            const Remove once = std::move(remove_);
            remove_ = nullptr;
            once();
        }
    }

private:
    Remove remove_;
};

/// Same order as the wire protocol states, so a cast between the two is exact.
enum class PacketPhase : std::uint8_t { Handshake, Status, Login, Configuration, Play };

inline constexpr std::int32_t kPacketPhaseCount = 5;

struct PacketAddress {
    PacketPhase phase = PacketPhase::Play;
    bool serverbound = false;
    std::int32_t id = 0;
};

/// Which packets a listener wants: any field left at -1 matches everything on that axis.
struct PacketScope {
    std::int32_t phase = -1;
    std::int32_t direction = -1;
    std::int32_t id = -1;

    [[nodiscard]] bool matches(const PacketAddress& address) const noexcept {
        return (phase < 0 || phase == static_cast<std::int32_t>(address.phase)) &&
               (direction < 0 || direction == (address.serverbound ? 1 : 0)) && (id < 0 || id == address.id);
    }
};

/// The live connection a packet travels on; valid only while the packet is being delivered or the owner holds it.
class PacketEndpoint {
public:
    PacketEndpoint() = default;
    PacketEndpoint(const PacketEndpoint&) = delete;
    PacketEndpoint& operator=(const PacketEndpoint&) = delete;
    PacketEndpoint(PacketEndpoint&&) = delete;
    PacketEndpoint& operator=(PacketEndpoint&&) = delete;
    virtual ~PacketEndpoint() = default;

    /// Writes a clientbound packet (id and body) in the connection's current state; `intercept` runs the listeners.
    virtual bool sendClientbound(std::int32_t id, std::span<const std::uint8_t> body, bool intercept) = 0;

    /// Handles a serverbound packet as if the client had sent it; `intercept` runs the listeners.
    virtual bool receiveServerbound(std::int32_t id, std::span<const std::uint8_t> body, bool intercept) = 0;

    virtual void close() = 0;

    [[nodiscard]] virtual std::uint64_t connectionId() const = 0;

    /// The client's address and port as the socket saw them (not a forwarded one).
    [[nodiscard]] virtual std::string address() const = 0;

    [[nodiscard]] virtual std::uint16_t port() const = 0;
};

struct PluginPacket {
    PacketAddress address;

    /// The vanilla name, filled only when a Lua listener is about to see the packet.
    std::string name;

    std::uint64_t connection = 0;

    /// The protocol version the client announced; 0 before the handshake.
    std::int32_t clientProtocol = 0;

    /// True when the client is older than the host speaks natively, so the bytes are in the host's shape.
    bool translated = false;

    std::string playerName;
    std::string playerUuid;

    /// The backend a proxy carries the packet to or from; empty on a server.
    std::string backend;

    /// The protocol the backend a proxy carries the packet to or from speaks; 0 on a server.
    std::int32_t backendProtocol = 0;

    /// False when the host will not take a cancel or an edit of this packet, which it still shows.
    bool editable = true;

    /// The packet body, without the id.
    std::vector<std::uint8_t> bytes;

    bool cancelled = false;

    std::optional<std::vector<std::uint8_t>> replacement;

    std::shared_ptr<PacketEndpoint> endpoint;

    [[nodiscard]] bool serverbound() const noexcept { return address.serverbound; }

    void clear() {
        cancelled = false;
        replacement.reset();
    }
};

using NativePacketListener = std::function<void(PluginPacket&)>;

class PluginPacketBus {
public:
    PluginPacketBus() = default;
    PluginPacketBus(const PluginPacketBus&) = delete;
    PluginPacketBus& operator=(const PluginPacketBus&) = delete;
    PluginPacketBus(PluginPacketBus&&) = delete;
    PluginPacketBus& operator=(PluginPacketBus&&) = delete;
    virtual ~PluginPacketBus() = default;

    /// Cheap and lock free: the hot path asks this for every packet and copies nothing while it answers no.
    [[nodiscard]] virtual bool listening(const PacketAddress& address) const noexcept = 0;

    /// Delivers to the listeners; false when one cancelled it. Safe from any thread.
    virtual bool fire(PluginPacket& packet) = 0;

    /// A native listener runs on the thread that carries the packet. An observer cannot cancel or edit.
    [[nodiscard]] virtual PluginHandle addNative(const PacketScope& scope, std::int32_t priority, bool observer,
                                                 NativePacketListener listener) = 0;

    /// Tells native listeners a connection is gone.
    virtual void connectionClosed(std::uint64_t connection) = 0;

    [[nodiscard]] virtual PluginHandle addCloseListener(std::function<void(std::uint64_t connection)> listener) = 0;
};
} // namespace basalt::wire::plugins
