#include <basalt/wire/multiversion/java/Link766Up.hpp>
#include <basalt/wire/multiversion/java/NewRegistries767.hpp>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/VarInt.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::wire::multiversion::java {
namespace {

using protocol::ProtocolState;

constexpr std::int32_t kConfigRegistryData = 7;
constexpr std::int32_t kConfigFinish = 3;
constexpr std::int32_t kDisguisedChat = 30;
constexpr std::int32_t kPlayerChat = 57;
constexpr std::int32_t kUpdateAttributes = 117;
constexpr std::int32_t kProjectilePower = 121;

struct KnownModifier {
    std::int64_t most;
    std::int64_t least;
    std::string_view id;
};

constexpr std::array kKnownModifiers = std::to_array<KnownModifier>({
    {INT64_C(-4483571535397864886), INT64_C(-5989644940537681742), "minecraft:armor.body"},
    {INT64_C(8144722948526719024), INT64_C(-7778190119041365872), "minecraft:effect.slowness"},
    {INT64_C(6967552254039378640), INT64_C(-9116175396973475259), "minecraft:enchantment.aqua_affinity"},
    {INT64_C(5279725409867744698), INT64_C(-5150363631200102632), "minecraft:attacking"},
    {INT64_C(148071211714102867), INT64_C(-7685811609035173472), "minecraft:attacking"},
    {INT64_C(6196088217904236654), INT64_C(-7493791321850887290), "minecraft:effect.mining_fatigue"},
    {INT64_C(-5084161844451524480), INT64_C(-8859020046251006329), "minecraft:enchantment.soul_speed"},
    {INT64_C(-7907339078496465106), INT64_C(-8112074600724210224), "minecraft:enchantment.swift_sneak"},
    {INT64_C(6688265815086220243), INT64_C(-6545541163342161890), "minecraft:drinking"},
    {INT64_C(8315164243412860989), INT64_C(-6631520853640075966), "minecraft:creative_mode_block_range"},
    {INT64_C(4389663563256579765), INT64_C(-4827163546944004714), "minecraft:enchantment.efficiency"},
    {INT64_C(6732612758648800940), INT64_C(-5145707699103688244), "minecraft:effect.health_boost"},
    {INT64_C(9079981369298536661), INT64_C(-6728494925450710401), "minecraft:covered"},
    {INT64_C(-1521481167610687786), INT64_C(-8630419745734927691), "minecraft:effect.absorption"},
    {INT64_C(-7473408062188862076), INT64_C(-5872005994337939597), "minecraft:creative_mode_entity_range"},
    {INT64_C(-3721396875562958315), INT64_C(-5317020504214661337), "minecraft:effect.unluck"},
    {INT64_C(-2861585646493481178), INT64_C(-6113244764726669811), "minecraft:armor.leggings"},
    {INT64_C(6718535547217657911), INT64_C(-5386630269401489641), "minecraft:enchantment.sweeping_edge"},
    {INT64_C(-7949229004988660584), INT64_C(-7828611303000832459), "minecraft:effect.speed"},
    {INT64_C(-8650171790042118250), INT64_C(-5749650997644763080), "minecraft:enchantment.soul_speed"},
    {INT64_C(551224288813600377), INT64_C(-8734740027710371860), "minecraft:enchantment.respiration"},
    {INT64_C(-7046399332347654691), INT64_C(-6723081531683397260), "minecraft:suffocating"},
    {INT64_C(7361814797886573596), INT64_C(-8641397326606817395), "minecraft:sprinting"},
    {INT64_C(-6972338111383059132), INT64_C(-8978659762232839026), "minecraft:armor.chestplate"},
    {INT64_C(-5371971015925809039), INT64_C(-6062243582569928137), "minecraft:enchantment.fire_protection"},
    {INT64_C(7245570952092733273), INT64_C(-8449101711440750679), "minecraft:effect.strength"},
    {INT64_C(-422425648963762075), INT64_C(-5756800103171642205), "minecraft:base_attack_speed"},
    {INT64_C(-4607081316629330256), INT64_C(-7008565754814018370), "minecraft:effect.jump_boost"},
    {INT64_C(271280981090454338), INT64_C(-8746077033958322898), "minecraft:effect.luck"},
    {INT64_C(2211131075215181206), INT64_C(-5513857709499300658), "minecraft:powder_snow"},
    {INT64_C(-8908768238899017377), INT64_C(-8313820693701227669), "minecraft:armor.boots"},
    {INT64_C(-5797418877589107702), INT64_C(-6181652684028920077), "minecraft:effect.haste"},
    {INT64_C(3086076556416732775), INT64_C(-5150312587563650736), "minecraft:armor.helmet"},
    {INT64_C(-5082757096938257406), INT64_C(-4891139119377885130), "minecraft:baby"},
    {INT64_C(2478452629826324956), INT64_C(-7247530463494186011), "minecraft:effect.weakness"},
    {INT64_C(4659420831966187055), INT64_C(-5191473055587376048), "minecraft:enchantment.blast_protection"},
    {INT64_C(7301951777949303281), INT64_C(-6753860660653972126), "minecraft:evil"},
    {INT64_C(8533189226688352746), INT64_C(-8254757081029716377), "minecraft:baby"},
    {INT64_C(1286946037536540352), INT64_C(-5768092872487507967), "minecraft:enchantment.depth_strider"},
    {INT64_C(-3801225194067177672), INT64_C(-6586624321849018929), "minecraft:base_attack_damage"},
});

std::string modifierId(std::int64_t most, std::int64_t least) {
    for (const KnownModifier& known : kKnownModifiers) {
        if (known.most == most && known.least == least) {
            return std::string{known.id};
        }
    }
    constexpr std::string_view kHex = "0123456789abcdef";
    std::string id = "minecraft:";
    for (int index = 0; index < 32; ++index) {
        const std::int64_t half = index < 16 ? most : least;
        const int shift = 60 - 4 * (index % 16);
        id += kHex[static_cast<std::size_t>((half >> shift) & 0xF)];
        if (index == 7 || index == 11 || index == 15 || index == 19) {
            id += '-';
        }
    }
    return id;
}

void copyComponent(io::ByteBuffer& in, io::ByteBuffer& out) {
    protocol::writeAnonymousNbt(out, protocol::readAnonymousNbt(in));
}

void copyOptionalComponent(io::ByteBuffer& in, io::ByteBuffer& out) {
    const bool present = protocol::readBoolean(in);
    protocol::writeBoolean(out, present);
    if (present) {
        copyComponent(in, out);
    }
}

void rewriteDisguisedChat(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    copyComponent(in, out);
    protocol::varInt::write(out, protocol::varInt::read(in) + 1);
    out.writeBytes(in.readable());
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

void rewritePlayerChat(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    out.writeBytes(in.readSpan(16));
    protocol::varInt::write(out, protocol::varInt::read(in));
    const bool signature = protocol::readBoolean(in);
    protocol::writeBoolean(out, signature);
    if (signature) {
        out.writeBytes(in.readSpan(256));
    }
    protocol::writeString(out, protocol::readString(in));
    out.writeLong(in.readLong());
    out.writeLong(in.readLong());
    const std::int32_t lastSeen = protocol::varInt::read(in);
    protocol::varInt::write(out, lastSeen);
    for (std::int32_t index = 0; index < lastSeen && !in.failed(); ++index) {
        const std::int32_t entry = protocol::varInt::read(in);
        protocol::varInt::write(out, entry);
        if (entry == 0) {
            out.writeBytes(in.readSpan(256));
        }
    }
    copyOptionalComponent(in, out);
    const std::int32_t filter = protocol::varInt::read(in);
    protocol::varInt::write(out, filter);
    if (filter == 2) {
        const std::int32_t words = protocol::varInt::read(in);
        protocol::varInt::write(out, words);
        for (std::int32_t index = 0; index < words && !in.failed(); ++index) {
            out.writeLong(in.readLong());
        }
    }
    protocol::varInt::write(out, protocol::varInt::read(in) + 1);
    copyComponent(in, out);
    copyOptionalComponent(in, out);
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

void rewriteAttributes(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    protocol::varInt::write(out, protocol::varInt::read(in));
    const std::int32_t count = protocol::varInt::read(in);
    protocol::varInt::write(out, count);
    for (std::int32_t index = 0; index < count && !in.failed(); ++index) {
        protocol::varInt::write(out, protocol::varInt::read(in));
        out.writeDouble(in.readDouble());
        const std::int32_t modifiers = protocol::varInt::read(in);
        protocol::varInt::write(out, modifiers);
        for (std::int32_t each = 0; each < modifiers && !in.failed(); ++each) {
            const std::int64_t most = in.readLong();
            const std::int64_t least = in.readLong();
            protocol::writeString(out, modifierId(most, least));
            out.writeDouble(in.readDouble());
            out.writeByte(in.readByte());
        }
    }
    if (!in.failed()) {
        packet.body = out.toVector();
    }
}

void rewriteProjectilePower(MutablePacket& packet) {
    io::ByteBuffer in = io::ByteBuffer::wrap(packet.body);
    io::ByteBuffer out;
    protocol::varInt::write(out, protocol::varInt::read(in));
    const double x = in.readDouble();
    const double y = in.readDouble();
    const double z = in.readDouble();
    if (in.failed()) {
        return;
    }
    out.writeDouble(std::sqrt(x * x + y * y + z * z));
    packet.body = out.toVector();
}

} // namespace

bool Link766Up::clientbound(ProtocolState state, MutablePacket& packet) {
    if (state == ProtocolState::Configuration) {
        if (packet.id == kConfigFinish) {
            const std::vector<std::uint8_t> finishBody = packet.body;
            const auto added = registriesAddedIn1_21();
            packet.overrideId(kConfigRegistryData);
            packet.body.assign(added[0].begin(), added[0].end());
            for (std::size_t index = 1; index < added.size(); ++index) {
                packet.forwarded.emplace_back(kConfigRegistryData,
                                              std::vector<std::uint8_t>{added[index].begin(), added[index].end()});
            }
            packet.forwarded.emplace_back(kConfigFinish, finishBody);
        }
        return true;
    }
    if (state != ProtocolState::Play) {
        return true;
    }
    switch (packet.id) {
    case kDisguisedChat:
        rewriteDisguisedChat(packet);
        break;
    case kPlayerChat:
        rewritePlayerChat(packet);
        break;
    case kUpdateAttributes:
        rewriteAttributes(packet);
        break;
    case kProjectilePower:
        rewriteProjectilePower(packet);
        break;
    default:
        break;
    }
    return true;
}

} // namespace basalt::wire::multiversion::java
