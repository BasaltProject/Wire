#pragma once

#include <basalt/wire/lua/LuaRef.hpp>
#include <basalt/wire/plugins/PluginPackets.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace basalt::wire::plugins {

/// The packet bus both hosts share. Lua listeners belong to an opaque owner the host gives back when it delivers.
class PacketRegistry : public PluginPacketBus {
public:
    /// Returns false when the listener could not run; a listener added with failClosed then cancels the packet.
    using Deliver = std::function<bool(const void* owner, const lua::LuaRef& fn, PluginPacket& packet)>;

    /// Gives a Lua listener the vanilla name of the packet it is about to see.
    using Namer = std::function<std::string(const PacketAddress&)>;

    PacketRegistry(Deliver deliver, Namer namer) : deliver_(std::move(deliver)), namer_(std::move(namer)) {}

    [[nodiscard]] PluginHandle add(const void* owner, const PacketScope& scope, lua::LuaRef fn, std::int32_t priority,
                                   bool observer, bool failClosed = false);

    [[nodiscard]] bool listening(const PacketAddress& address) const noexcept override;

    bool fire(PluginPacket& packet) override;

    [[nodiscard]] PluginHandle addNative(const PacketScope& scope, std::int32_t priority, bool observer,
                                         NativePacketListener listener) override;

    void connectionClosed(std::uint64_t connection) override;

    /// Returns once no delivery that began before the call is still running; remove listeners first.
    void drain() const;

    [[nodiscard]] PluginHandle addCloseListener(std::function<void(std::uint64_t connection)> listener) override;

private:
    struct Listener {
        PacketScope scope;
        const void* owner = nullptr;
        lua::LuaRef fn;
        NativePacketListener native;
        std::int32_t priority = 0;
        bool observer = false;
        bool failClosed = false;
        std::int64_t id = 0;
    };

    using Snapshot = std::vector<std::shared_ptr<const Listener>>;

    static constexpr std::int32_t kCountedIds = 256;

    PluginHandle insert(std::shared_ptr<Listener> listener);
    void remove(std::int64_t id);
    void count(const PacketScope& scope, std::int32_t delta);

    Deliver deliver_;
    Namer namer_;
    std::mutex mutex_;
    std::shared_ptr<const Snapshot> listeners_ = std::make_shared<const Snapshot>();
    std::vector<std::pair<std::int64_t, std::function<void(std::uint64_t)>>> closers_;
    std::int64_t nextId_ = 0;

    mutable std::atomic<std::int32_t> inFlight_{0};
    std::atomic<std::int32_t> everything_{0};
    std::array<std::atomic<std::int32_t>, 2> perDirection_{};
    std::array<std::atomic<std::int32_t>, 2 * kPacketPhaseCount> perPhase_{};
    std::array<std::atomic<std::int32_t>, 2 * kPacketPhaseCount * kCountedIds> perId_{};
    std::atomic<std::int32_t> oddIds_{0};
};
} // namespace basalt::wire::plugins
