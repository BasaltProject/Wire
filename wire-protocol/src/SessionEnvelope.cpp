#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/SessionEnvelope.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

namespace basalt::wire::protocol::sessionEnvelope {
namespace {

constexpr std::uint8_t kKindData = 0;
constexpr std::uint8_t kKindControl = 1;
constexpr std::uint8_t kControlOpen = 0;
constexpr std::uint8_t kControlOpened = 1;
constexpr std::uint8_t kControlClose = 2;

} // namespace

std::vector<std::uint8_t> encodeData(std::span<const std::uint8_t> packet) {
    std::vector<std::uint8_t> frame;
    frame.reserve(packet.size() + 1);
    frame.push_back(kKindData);
    frame.insert(frame.end(), packet.begin(), packet.end());
    return frame;
}

std::vector<std::uint8_t> encodeControl(const Control& control) {
    io::ByteBuffer out;
    out.writeByte(kKindControl);
    if (const auto* open = std::get_if<Open>(&control)) {
        out.writeByte(kControlOpen);
        out.writeByte(open->version);
        out.writeByte(static_cast<std::uint8_t>(open->edition));
        varInt::write(out, open->protocol);
    } else if (const auto* opened = std::get_if<Opened>(&control)) {
        out.writeByte(kControlOpened);
        out.writeByte(opened->version);
    } else {
        out.writeByte(kControlClose);
        out.writeByte(static_cast<std::uint8_t>(std::get<Close>(control).reason));
    }
    return out.toVector();
}

std::expected<Frame, DecodeError> decode(std::span<const std::uint8_t> frame) {
    if (frame.empty())
        return std::unexpected(DecodeError::kEmpty);
    if (frame[0] == kKindData) {
        if (frame.size() - 1 > kMaxDataBytes)
            return std::unexpected(DecodeError::kTooLarge);
        return Data{frame.subspan(1)};
    }
    if (frame[0] != kKindControl)
        return std::unexpected(DecodeError::kUnknownKind);
    if (frame.size() > kMaxControlBytes)
        return std::unexpected(DecodeError::kTooLarge);
    io::ByteBuffer in = io::ByteBuffer::wrap(std::vector<std::uint8_t>{frame.begin() + 1, frame.end()});
    const std::uint8_t type = in.readByte();
    if (in.failed())
        return std::unexpected(DecodeError::kMalformed);
    if (type == kControlOpen) {
        Open open;
        open.version = in.readByte();
        const std::uint8_t edition = in.readByte();
        open.protocol = varInt::read(in);
        if (in.failed() || in.readableBytes() != 0 || edition > static_cast<std::uint8_t>(Edition::Bedrock) ||
            open.protocol < 0)
            return std::unexpected(DecodeError::kMalformed);
        open.edition = static_cast<Edition>(edition);
        return Control{open};
    }
    if (type == kControlOpened) {
        Opened opened;
        opened.version = in.readByte();
        if (in.failed() || in.readableBytes() != 0)
            return std::unexpected(DecodeError::kMalformed);
        return Control{opened};
    }
    if (type == kControlClose) {
        const std::uint8_t reason = in.readByte();
        if (in.failed() || in.readableBytes() != 0 || reason > static_cast<std::uint8_t>(CloseReason::Superseded))
            return std::unexpected(DecodeError::kMalformed);
        return Control{Close{static_cast<CloseReason>(reason)}};
    }
    return std::unexpected(DecodeError::kUnknownControl);
}

} // namespace basalt::wire::protocol::sessionEnvelope
