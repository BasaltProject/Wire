#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/io/Compression.hpp>
#include <basalt/wire/io/EventLoop.hpp>
#include <basalt/wire/io/Platform.hpp>
#include <basalt/wire/io/TcpSocket.hpp>
#include <basalt/wire/io/ZipReader.hpp>

#include <doctest.h>

#include <atomic>
#include <cstdint>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

using namespace basalt::wire::io;

namespace {

std::vector<std::uint8_t> sampleBytes(std::size_t size) {
    std::vector<std::uint8_t> out(size);
    for (std::size_t i = 0; i < size; ++i) {
        out[i] = static_cast<std::uint8_t>((i * 7) % 251);
    }
    return out;
}

} // namespace

TEST_CASE("ByteBuffer round trips every primitive in big-endian order") {
    ByteBuffer buffer;
    buffer.writeByte(0xAB);
    buffer.writeShort(-1234);
    buffer.writeInt(-70000);
    buffer.writeLong(1234567890123LL);
    buffer.writeFloat(3.5F);
    buffer.writeDouble(2.718281828459045);
    const std::vector<std::uint8_t> payload = {1, 2, 3, 4};
    buffer.writeBytes(payload);

    CHECK(buffer.readByte() == 0xAB);
    CHECK(buffer.readShort() == -1234);
    CHECK(buffer.readInt() == -70000);
    CHECK(buffer.readLong() == 1234567890123LL);
    CHECK(buffer.readFloat() == 3.5F);
    CHECK(buffer.readDouble() == 2.718281828459045);
    CHECK(buffer.readBytes(4) == payload);
    CHECK(buffer.readableBytes() == 0);
    CHECK_FALSE(buffer.failed());
}

TEST_CASE("ByteBuffer reports a short read instead of running off the end") {
    ByteBuffer buffer;
    buffer.writeByte(1);

    CHECK(buffer.readByte() == 1);
    CHECK_FALSE(buffer.failed());
    CHECK(buffer.readInt() == 0);
    CHECK(buffer.failed());
    CHECK(buffer.readByte() == 0);
    CHECK(buffer.failed());
    buffer.clearFailure();
    CHECK_FALSE(buffer.failed());
}

TEST_CASE("gzip produces a real gzip stream and reads it back") {
    const std::vector<std::uint8_t> data = sampleBytes(4096);
    const auto compressed = gzip::compress(data);
    REQUIRE(compressed.has_value());
    REQUIRE(compressed->size() > 2);
    CHECK((*compressed)[0] == 0x1F);
    CHECK((*compressed)[1] == 0x8B);
    CHECK(gzip::decompress(*compressed).value() == data);
}

TEST_CASE("the zip reader pulls stored and deflated entries out of an archive") {
    const std::string storedName = "stored.txt";
    const std::string storedBody = "hello";

    std::vector<std::uint8_t> archive;
    const auto put16 = [&archive](std::uint16_t value) {
        archive.push_back(static_cast<std::uint8_t>(value));
        archive.push_back(static_cast<std::uint8_t>(value >> 8));
    };
    const auto put32 = [&archive](std::uint32_t value) {
        for (int i = 0; i < 4; ++i) {
            archive.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
        }
    };
    const auto putBytes = [&archive](std::string_view text) {
        archive.insert(archive.end(), text.begin(), text.end());
    };

    const std::uint32_t localHeaderOffset = 0;
    put32(0x04034b50);
    put16(20);
    put16(0);
    put16(0);
    put16(0);
    put16(0);
    put32(0);
    put32(static_cast<std::uint32_t>(storedBody.size()));
    put32(static_cast<std::uint32_t>(storedBody.size()));
    put16(static_cast<std::uint16_t>(storedName.size()));
    put16(0);
    putBytes(storedName);
    putBytes(storedBody);

    const auto directoryOffset = static_cast<std::uint32_t>(archive.size());
    put32(0x02014b50);
    put16(20);
    put16(20);
    put16(0);
    put16(0);
    put16(0);
    put16(0);
    put32(0);
    put32(static_cast<std::uint32_t>(storedBody.size()));
    put32(static_cast<std::uint32_t>(storedBody.size()));
    put16(static_cast<std::uint16_t>(storedName.size()));
    put16(0);
    put16(0);
    put16(0);
    put16(0);
    put32(0);
    put32(localHeaderOffset);
    putBytes(storedName);

    const auto directorySize = static_cast<std::uint32_t>(archive.size() - directoryOffset);
    put32(0x06054b50);
    put16(0);
    put16(0);
    put16(1);
    put16(1);
    put32(directorySize);
    put32(directoryOffset);
    put16(0);

    const auto reader = ZipReader::open(archive);
    REQUIRE(reader.has_value());
    CHECK(reader->entryNames() == std::vector<std::string>{storedName});
    const auto entry = reader->read(storedName);
    REQUIRE(entry.has_value());
    CHECK(std::string(entry->begin(), entry->end()) == storedBody);
    CHECK(reader->read("missing").error() == ZipError::EntryNotFound);

    CHECK(ZipReader::open(std::vector<std::uint8_t>{1, 2, 3}).error() == ZipError::NotAZipFile);
}

TEST_CASE("zstd round trips a whole region's worth of data") {
    const std::vector<std::uint8_t> data = sampleBytes(1024 * 1024);

    const auto compressed = zstd::compress(data);
    REQUIRE(compressed.has_value());
    CHECK(compressed->size() < data.size());
    CHECK(zstd::decompress(*compressed).value() == data);

    CHECK(zstd::compress(std::vector<std::uint8_t>{}).value().empty());
    CHECK_FALSE(zstd::decompress(std::vector<std::uint8_t>{0xDE, 0xAD, 0xBE, 0xEF}).has_value());
}

TEST_CASE("the streaming reader hands back exactly what was asked for") {
    const std::vector<std::uint8_t> original = sampleBytes(300 * 1024);
    const std::vector<std::uint8_t> compressed = zstd::compress(original).value();

    zstd::StreamDecompressor reader{compressed};
    REQUIRE(reader.valid());
    std::vector<std::uint8_t> head(100);
    std::vector<std::uint8_t> middle(200 * 1024);
    std::vector<std::uint8_t> tail(original.size() - head.size() - middle.size());
    REQUIRE(reader.readFully(head));
    CHECK_FALSE(reader.frameComplete());
    REQUIRE(reader.readFully(middle));
    REQUIRE(reader.readFully(tail));
    CHECK(reader.frameComplete());

    std::vector<std::uint8_t> rebuilt;
    rebuilt.insert(rebuilt.end(), head.begin(), head.end());
    rebuilt.insert(rebuilt.end(), middle.begin(), middle.end());
    rebuilt.insert(rebuilt.end(), tail.begin(), tail.end());
    CHECK(rebuilt == original);

    std::vector<std::uint8_t> past(16);
    CHECK_FALSE(reader.readFully(past));
}

TEST_CASE("nonblocking connections report readiness and distinguish would-block from EOF") {
    auto listener = TcpListener::bind(0);
    REQUIRE(listener);
    REQUIRE(listener->setNonBlocking());
    const auto empty = listener->accept();
    REQUIRE_FALSE(empty);
    CHECK(empty.error().kind == SocketError::Kind::WouldBlock);
    const auto port = listener->localPort();
    REQUIRE(port);
    auto client = TcpConnection::connectNonBlocking("127.0.0.1", *port);
    REQUIRE(client);
    EventLoop loop;
    REQUIRE(loop.watch(client->nativeHandle(), {false, true}, 41));
    auto ready = loop.wait(2000);
    REQUIRE(ready);
    REQUIRE(ready->size() == 1);
    CHECK(ready->front().token == 41);
    CHECK(ready->front().writable);
    const auto connected = client->finishConnect();
    REQUIRE(connected);
    REQUIRE(*connected);
    auto peer = listener->accept();
    REQUIRE(peer);
    REQUIRE(peer->setNonBlocking());
    std::array<std::uint8_t, 16> bytes{};
    const auto nothing = client->read(bytes);
    REQUIRE_FALSE(nothing);
    CHECK(nothing.error().kind == SocketError::Kind::WouldBlock);
    REQUIRE(loop.watch(client->nativeHandle(), {true, false}, 42));
    ready = loop.wait(0);
    REQUIRE(ready);
    CHECK(ready->empty());
    REQUIRE(peer->writeSome(std::array<std::uint8_t, 3>{1, 2, 3}));
    ready = loop.wait(2000);
    REQUIRE(ready);
    REQUIRE(ready->size() == 1);
    CHECK(ready->front().token == 42);
    CHECK(ready->front().readable);
    const auto received = client->read(bytes);
    REQUIRE(received);
    CHECK(*received == 3);
    peer->close();
    ready = loop.wait(2000);
    REQUIRE(ready);
    REQUIRE_FALSE(ready->empty());
    const auto eof = client->read(bytes);
    REQUIRE(eof);
    CHECK(*eof == 0);
    loop.unwatch(client->nativeHandle());
    CHECK(loop.watchedCount() == 0);
    ready = loop.wait(0);
    REQUIRE(ready);
    CHECK(ready->empty());
}
