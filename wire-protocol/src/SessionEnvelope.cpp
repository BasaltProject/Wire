#include <basalt/wire/io/ByteBuffer.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/SessionEnvelope.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

namespace basalt::wire::protocol::sessionEnvelope {
namespace {

constexpr std::uint8_t kKindData = 0;
constexpr std::uint8_t kKindControl = 1;
constexpr std::uint8_t kControlOpen = 0;
constexpr std::uint8_t kControlOpened = 1;
constexpr std::uint8_t kControlClose = 2;
constexpr std::uint8_t kControlIdentity = 3;
constexpr std::uint8_t kControlDisconnect = 4;
constexpr std::int32_t kMaxDisconnectText = 400;
constexpr std::int32_t kMaxSkinSide = 1024;
constexpr std::int32_t kMaxText = 65536;

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
    } else if (const auto* identity = std::get_if<BedrockIdentity>(&control)) {
        out.writeByte(kControlIdentity);
        writeString(out, identity->gamertag);
        writeString(out, identity->xuid);
        out.writeByte(identity->authenticated ? 1 : 0);
        writeString(out, identity->address);
        writeString(out, identity->skinId);
        writeString(out, identity->resourcePatch);
        varInt::write(out, identity->skinWidth);
        varInt::write(out, identity->skinHeight);
        varInt::write(out, static_cast<std::int32_t>(identity->skinPixels.size()));
        out.writeBytes(identity->skinPixels);
    } else if (const auto* disconnect = std::get_if<Disconnect>(&control)) {
        out.writeByte(kControlDisconnect);
        writeString(out, disconnect->message);
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
    if (frame.size() > kMaxDataBytes + 1 ||
        (frame.size() > kMaxControlBytes && frame.size() > 1 && frame[1] != kControlIdentity))
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
    if (type == kControlDisconnect) {
        Disconnect disconnect;
        disconnect.message = readString(in, kMaxDisconnectText);
        if (in.failed() || in.readableBytes() != 0)
            return std::unexpected(DecodeError::kMalformed);
        return Control{std::move(disconnect)};
    }
    if (type == kControlIdentity) {
        BedrockIdentity identity;
        identity.gamertag = readString(in, kMaxText);
        identity.xuid = readString(in, kMaxText);
        identity.authenticated = in.readByte() != 0;
        identity.address = readString(in, kMaxText);
        identity.skinId = readString(in, kMaxText);
        identity.resourcePatch = readString(in, kMaxText);
        identity.skinWidth = varInt::read(in);
        identity.skinHeight = varInt::read(in);
        const std::int32_t pixels = varInt::read(in);
        if (in.failed() || identity.skinWidth < 0 || identity.skinHeight < 0 || identity.skinWidth > kMaxSkinSide ||
            identity.skinHeight > kMaxSkinSide || pixels < 0 ||
            static_cast<std::int64_t>(pixels) !=
                static_cast<std::int64_t>(identity.skinWidth) * identity.skinHeight * 4 ||
            in.readableBytes() != static_cast<std::size_t>(pixels))
            return std::unexpected(DecodeError::kMalformed);
        const auto bytes = in.readSpan(static_cast<std::size_t>(pixels));
        identity.skinPixels.assign(bytes.begin(), bytes.end());
        return Control{std::move(identity)};
    }
    return std::unexpected(DecodeError::kUnknownControl);
}

} // namespace basalt::wire::protocol::sessionEnvelope
