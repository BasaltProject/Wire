#include <basalt/wire/bedrock/BedrockJavaBlockEntities.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <string>
#include <string_view>

namespace basalt::wire::bedrock {
namespace {

[[nodiscard]] std::string plainSide(const nbt::NbtCompound* side) {
    const nbt::NbtList* messages = side == nullptr ? nullptr : side->getList("messages");
    if (messages == nullptr) {
        return {};
    }
    std::string text;
    bool first = true;
    for (const nbt::NbtTag& line : *messages) {
        if (!first) {
            text += '\n';
        }
        first = false;
        text += text::toPlainText(text::componentNbt::deserialize(line));
    }
    return text;
}

} // namespace

std::optional<JavaSignText> readJavaSign(const nbt::NbtCompound& data) {
    return JavaSignText{.front = plainSide(data.getCompound("front_text")),
                        .back = plainSide(data.getCompound("back_text")),
                        .waxed = data.getByte("is_waxed") != 0};
}

bool writeJavaBlockEntity(io::ByteBuffer& tags, const std::int32_t x, const std::int32_t y, const std::int32_t z,
                          const std::int32_t javaType, const nbt::NbtCompound& data) {
    if (javaType != kJavaSignType && javaType != kJavaHangingSignType) {
        return false;
    }
    const std::optional<JavaSignText> sign = readJavaSign(data);
    writeSignTag(tags, x, y, z, sign->front, sign->back, sign->waxed);
    return true;
}

} // namespace basalt::wire::bedrock
