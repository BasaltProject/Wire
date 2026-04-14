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
    // The flag is sticky, so one check after a whole packet is enough.
    CHECK(buffer.readByte() == 0);
    CHECK(buffer.failed());
    buffer.clearFailure();
    CHECK_FALSE(buffer.failed());
}

TEST_CASE("ByteBuffer reuses its storage across packets") {
    ByteBuffer buffer;
    buffer.writeInt(1);
    buffer.writeInt(2);
    CHECK(buffer.readInt() == 1);

    buffer.discardReadBytes();
    CHECK(buffer.readerIndex() == 0);
    CHECK(buffer.readableBytes() == 4);
    CHECK(buffer.readInt() == 2);
}

TEST_CASE("zlib compresses straight into a destination buffer") {
    const std::vector<std::uint8_t> data = sampleBytes(8192);

    ByteBuffer compressed;
    compressed.writeInt(0x5A5A5A5A);
    REQUIRE(zlib::compressInto(data, compressed).has_value());
    // The header written before it is untouched: the compressor appends, it does not own the buffer.
    CHECK(compressed.readInt() == 0x5A5A5A5A);
    const std::vector<std::uint8_t> body = compressed.toVector();
    CHECK(body.size() < data.size());

    ByteBuffer restored;
    REQUIRE(zlib::decompressInto(body, data.size(), restored).has_value());
    CHECK(restored.toVector() == data);
    CHECK(zlib::decompressUnknownSize(body).value() == data);
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

TEST_CASE("corrupt compressed input is refused, not crashed on") {
    const std::vector<std::uint8_t> garbage = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x11};
    CHECK_FALSE(zlib::decompress(garbage, 1024).has_value());
    CHECK_FALSE(gzip::decompress(garbage).has_value());
    CHECK_FALSE(rawDeflate::decompress(garbage, 1024).has_value());
    CHECK(zlib::decompress(garbage, kMaxDecompressedSize + 1).error() == CompressionError::TooLarge);
}

TEST_CASE("IPv4 literals parse and format back") {
    CHECK(parseIpv4("127.0.0.1").has_value());
    CHECK(formatIpv4(*parseIpv4("192.168.1.42")) == "192.168.1.42");
    CHECK_FALSE(parseIpv4("256.0.0.1").has_value());
    CHECK_FALSE(parseIpv4("1.2.3").has_value());
    CHECK_FALSE(parseIpv4("not an address").has_value());
    CHECK_FALSE(parseIpv4("1.2.3.4.5").has_value());
}

TEST_CASE("a listener accepts a connection and the two exchange bytes") {
    auto listener = TcpListener::bind(0);
    REQUIRE(listener.has_value());
    const auto port = listener->localPort();
    REQUIRE(port.has_value());

    EventLoop loop;
    loop.watch(listener->nativeHandle());

    auto client = TcpConnection::connect("127.0.0.1", *port);
    REQUIRE(client.has_value());

    const auto ready = loop.waitReadable(2000);
    REQUIRE(ready.has_value());
    REQUIRE(ready->size() == 1);
    CHECK(ready->front() == listener->nativeHandle());

    auto server = listener->accept();
    REQUIRE(server.has_value());
    CHECK(server->remoteAddress() == "127.0.0.1");

    const std::vector<std::uint8_t> message = {'b', 'a', 's', 'a', 'l', 't'};
    REQUIRE(client->write(message).has_value());

    std::vector<std::uint8_t> received(message.size());
    const auto count = server->read(received);
    REQUIRE(count.has_value());
    CHECK(*count == message.size());
    CHECK(received == message);

    // A closed peer reads as end of stream, not as an error.
    client->close();
    const auto afterClose = server->read(received);
    REQUIRE(afterClose.has_value());
    CHECK(*afterClose == 0);
}

TEST_CASE("connecting to something that is not an address fails cleanly") {
    const auto failed = TcpConnection::connect("localhost", 1);
    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error().kind == SocketError::Kind::InvalidAddress);
}

TEST_CASE("the zip reader pulls stored and deflated entries out of an archive") {
    // A minimal archive built by hand, so the test carries no third-party binary and no jar.
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

TEST_CASE("the streaming compressor never holds the whole input") {
    // The point of it: a region is written from the blobs already in hand, one at a time, and the
    // output leaves through the sink as it is produced.
    std::vector<std::uint8_t> compressed;
    {
        zstd::StreamCompressor compressor{[&compressed](std::span<const std::uint8_t> block) {
            compressed.insert(compressed.end(), block.begin(), block.end());
        }};
        REQUIRE(compressor.valid());

        for (int piece = 0; piece < 16; ++piece) {
            REQUIRE(compressor.feed(sampleBytes(64 * 1024)));
        }
        REQUIRE(compressor.finish());
    }

    std::vector<std::uint8_t> expected;
    for (int piece = 0; piece < 16; ++piece) {
        const std::vector<std::uint8_t> chunk = sampleBytes(64 * 1024);
        expected.insert(expected.end(), chunk.begin(), chunk.end());
    }
    CHECK(compressed.size() < expected.size());
    CHECK(zstd::decompress(compressed).value() == expected);
}

TEST_CASE("the streaming reader hands back exactly what was asked for") {
    // The reading half: a region is a table followed by the blobs it describes, and whoever wants
    // one blob reads past the table and stops.
    const std::vector<std::uint8_t> original = sampleBytes(300 * 1024);
    const std::vector<std::uint8_t> compressed = zstd::compress(original).value();

    zstd::StreamDecompressor reader{compressed};
    REQUIRE(reader.valid());
    // Smaller than one output block, then larger than one, then the rest: the reader has to hold
    // what it did not hand out and cross its own buffer boundary without losing a byte.
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

    // Asking for more than the frame holds says no rather than handing back whatever was there.
    std::vector<std::uint8_t> past(16);
    CHECK_FALSE(reader.readFully(past));
}

TEST_CASE("a stream that ends early, and one that never was") {
    const std::vector<std::uint8_t> original = sampleBytes(4096);
    const std::vector<std::uint8_t> compressed = zstd::compress(original).value();

    zstd::StreamDecompressor reader{compressed};
    std::vector<std::uint8_t> tooMuch(original.size() + 1);
    CHECK_FALSE(reader.readFully(tooMuch));

    // Stopping short of the end is not an error, but it is not a finished frame either: a format
    // whose table said less than the stream holds has to be able to tell.
    zstd::StreamDecompressor partial{compressed};
    std::vector<std::uint8_t> half(2048);
    CHECK(partial.readFully(half));
    CHECK_FALSE(partial.frameComplete());

    const std::vector<std::uint8_t> notAFrame{0xDE, 0xAD, 0xBE, 0xEF};
    zstd::StreamDecompressor rubbish{notAFrame};
    std::vector<std::uint8_t> nothing(8);
    CHECK_FALSE(rubbish.readFully(nothing));
    CHECK_FALSE(rubbish.valid());
    CHECK_FALSE(rubbish.frameComplete());
}

TEST_CASE("the platform helpers answer or say they cannot") {
    // The resident set is what an operator's own `top` shows, so a running process has one.
    CHECK(residentSetBytes() > 0);

    const auto cores = cpuCoreCount();
    REQUIRE(cores.has_value());
    CHECK(*cores >= 1);

    const auto cpu = processCpuTime();
    REQUIRE(cpu.has_value());
    CHECK(cpu->count() >= 0);

    // A shutdown signal is a flag, polled, never acted on from the handler itself.
    shutdownSignal::reset();
    CHECK_FALSE(shutdownSignal::isRequested());

    // Raw mode is refused when standard input is not a terminal, which is not a failure: under a
    // test runner that is exactly the situation, and restoring is safe either way.
    static_cast<void>(terminal::enableRawMode());
    terminal::restoreMode();
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

TEST_CASE("a stalled receiver never makes a nonblocking writer wait for the whole payload") {
    auto listener = TcpListener::bind(0);
    REQUIRE(listener);
    const auto port = listener->localPort();
    REQUIRE(port);
    auto client = TcpConnection::connect("127.0.0.1", *port);
    REQUIRE(client);
    auto peer = listener->accept();
    REQUIRE(peer);
    REQUIRE(client->setNonBlocking());
    const std::vector<std::uint8_t> bytes(1024 * 1024, 0x5A);
    bool blocked = false;
    for (int attempt = 0; attempt < 64; ++attempt) {
        const auto sent = client->writeSome(bytes);
        if (!sent) {
            CHECK(sent.error().kind == SocketError::Kind::WouldBlock);
            blocked = true;
            break;
        }
        CHECK(*sent > 0);
        CHECK(*sent <= bytes.size());
    }
    CHECK(blocked);
}

TEST_CASE("an idle event loop can be woken by a worker without leaking a readiness event") {
    EventLoop loop;
    std::atomic<int> workerError{0};
    std::jthread worker([&] {
        const auto woken = loop.wake();
        if (!woken) {
            workerError.store(woken.error());
        }
    });
    const auto ready = loop.wait(2000);
    REQUIRE(ready);
    CHECK(ready->empty());
    worker.join();
    CHECK(workerError.load() == 0);
    const auto drained = loop.wait(0);
    REQUIRE(drained);
    CHECK(drained->empty());
}

TEST_CASE("changing a readiness registration replaces its generation token") {
    auto listener = TcpListener::bind(0);
    REQUIRE(listener);
    const auto port = listener->localPort();
    REQUIRE(port);
    auto client = TcpConnection::connect("127.0.0.1", *port);
    REQUIRE(client);
    auto peer = listener->accept();
    REQUIRE(peer);
    EventLoop loop;
    REQUIRE(loop.watch(client->nativeHandle(), {false, true}, 1));
    loop.unwatch(client->nativeHandle());
    REQUIRE(loop.watch(client->nativeHandle(), {false, true}, 2));
    const auto ready = loop.wait(2000);
    REQUIRE(ready);
    REQUIRE(ready->size() == 1);
    CHECK(ready->front().token == 2);
    CHECK(loop.watchedCount() == 1);
}

TEST_CASE("resolved endpoints support numeric IPv4 and IPv6 without treating hostnames as literals") {
    const auto v4 = TcpAddress::literal("127.0.0.1", 25565);
    REQUIRE(v4);
    CHECK(v4->host() == "127.0.0.1");
    CHECK(v4->port() == 25565);
    const auto v6 = TcpAddress::literal("::1", 25565);
    REQUIRE(v6);
    CHECK(v6->host() == "::1");
    CHECK(v6->port() == 25565);
    CHECK_FALSE(TcpAddress::literal("localhost", 25565));
    CHECK_FALSE(TcpAddress::literal("127.0.0.1.bad", 25565));
    CHECK_FALSE(TcpAddress::resolve(std::string{"localhost\0other", 15}, 25565));
    const auto local = TcpAddress::resolve("localhost", 25565);
    REQUIRE(local);
    REQUIRE_FALSE(local->empty());
    for (const auto& address : *local) {
        CHECK(address.port() == 25565);
    }
}

TEST_CASE("an IPv6 loopback listener supports nonblocking readiness and peer addressing") {
    const auto endpoint = TcpAddress::literal("::1", 0);
    REQUIRE(endpoint);
    auto listener = TcpListener::bind(*endpoint);
    REQUIRE(listener);
    const auto port = listener->localPort();
    REQUIRE(port);
    const auto remote = TcpAddress::literal("::1", *port);
    REQUIRE(remote);
    auto client = TcpConnection::connectNonBlocking(*remote);
    REQUIRE(client);
    EventLoop loop;
    REQUIRE(loop.watch(client->nativeHandle(), {false, true}, 1));
    const auto ready = loop.wait(2000);
    REQUIRE(ready);
    REQUIRE_FALSE(ready->empty());
    const auto connected = client->finishConnect();
    REQUIRE(connected);
    REQUIRE(*connected);
    auto peer = listener->accept();
    REQUIRE(peer);
    CHECK(peer->remoteAddress() == "::1");
    REQUIRE(client->writeSome(std::array<std::uint8_t, 1>{0x42}));
    std::array<std::uint8_t, 1> bytes{};
    const auto received = peer->read(bytes);
    REQUIRE(received);
    CHECK(*received == 1);
    CHECK(bytes[0] == 0x42);
}
