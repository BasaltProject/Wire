#include <basalt/wire/protocol/PacketNames.hpp>

#include <doctest.h>

using namespace basalt::wire::protocol;

TEST_CASE("packet names resolve by every spelling and round trip") {
    const auto chat = packetNames::resolve("serverbound.chat_command");
    REQUIRE(chat.has_value());
    CHECK(*chat == packetNames::Address{packetNames::Phase::kPlay, true, 7});
    CHECK(packetNames::resolve("serverbound.play.0x7") == chat);
    CHECK(packetNames::of(*chat) == "serverbound.chat_command");
    CHECK_FALSE(packetNames::resolve("serverbound.no_such_packet"));
}
