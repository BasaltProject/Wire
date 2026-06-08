#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/wire/plugins/PacketRegistry.hpp>

#include <doctest.h>

#include <string>

using namespace basalt::wire::plugins;

TEST_CASE("the registry costs nothing without a listener and honours scope, order, cancel and frozen packets") {
    PacketRegistry registry{{}, {}};
    const PacketAddress chat{PacketPhase::Play, true, 7};
    CHECK_FALSE(registry.listening(chat));

    std::string order;
    PluginHandle late = registry.addNative({static_cast<std::int32_t>(PacketPhase::Play), 1, 7}, 3, false,
                                           [&](PluginPacket&) { order += 'b'; });
    PluginHandle early =
        registry.addNative({static_cast<std::int32_t>(PacketPhase::Play), 1, 7}, 1, false, [&](PluginPacket& packet) {
            order += 'a';
            packet.cancelled = true;
        });
    PluginHandle watcher = registry.addNative({-1, -1, -1}, 0, true, [&](PluginPacket& packet) {
        order += 'w';
        packet.cancelled = false;
    });
    CHECK(registry.listening(chat));
    CHECK(registry.listening({PacketPhase::Login, false, 0}));

    PluginPacket packet;
    packet.address = chat;
    CHECK_FALSE(registry.fire(packet));
    CHECK(order == "aw");

    early.remove();
    late.remove();
    watcher.remove();
    CHECK_FALSE(registry.listening(chat));

    PluginHandle edit =
        registry.addNative({-1, -1, -1}, 0, false, [](PluginPacket& edited) { edited.cancelled = true; });
    packet.editable = false;
    CHECK(registry.fire(packet));
}
