#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/multiversion/LoginShapes.hpp>
#include <basalt/wire/multiversion/ProtocolVersions.hpp>
#include <basalt/wire/multiversion/java/LoginRewriter.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/login/LoginPackets.hpp>

namespace basalt::wire::multiversion::java {
namespace {

namespace login = protocol::login;
namespace versions = protocolVersions;

} // namespace

bool LoginRewriter::clientbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Login) {
        return true;
    }
    if (packet.id == login::HelloPacket::kId && protocol_ < versions::kV1_20_5 && !packet.body.empty()) {
        // The flag that says whether to authenticate came with 1.20.5.
        packet.body.pop_back();
        return true;
    }
    if (packet.id != login::LoginFinishedPacket::kId || protocol_ >= versions::kV1_20_2) {
        return true;
    }
    // By now the links have left uuid, name and properties; older clients read less of it and a different uuid.
    io::ByteBuffer in{packet.body.size()};
    in.writeBytes(packet.body);
    login::LoginFinishedPacket finished;
    const auto id = protocol::readUuid(in);
    const auto name = protocol::readString(in, 16);
    if (in.failed()) {
        return false;
    }
    finished.gameProfile.id = id;
    finished.gameProfile.name = name;
    io::ByteBuffer out;
    if (protocol_ >= versions::kV1_19) {
        return true;
    }
    loginShapes::writeLoginSuccess(out, protocol_, finished);
    const auto shaped = out.readable();
    packet.body.assign(shaped.begin(), shaped.end());
    return true;
}

bool LoginRewriter::serverbound(protocol::ProtocolState state, MutablePacket& packet) {
    if (state != protocol::ProtocolState::Login || protocol_ >= versions::kV1_20_2) {
        return true;
    }
    io::ByteBuffer in{packet.body.size()};
    in.writeBytes(packet.body);
    if (packet.id == login::LoginStartPacket::kId) {
        const auto name = loginShapes::readClientLoginName(protocol_, in);
        if (!name) {
            return false;
        }
        io::ByteBuffer out;
        login::LoginStartPacket{*name, common::Uuid::offlinePlayer(*name)}.write(out);
        const auto native = out.readable();
        packet.body.assign(native.begin(), native.end());
        return true;
    }
    if (packet.id == login::KeyPacket::kId) {
        const auto key = loginShapes::readEncryptionResponse(protocol_, in);
        if (!key || in.readableBytes() != 0) {
            return false;
        }
        io::ByteBuffer out;
        key->write(out);
        const auto native = out.readable();
        packet.body.assign(native.begin(), native.end());
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
