#include <basalt/wire/nbt/NbtType.hpp>

namespace basalt::wire::nbt {

std::string_view nbtTypeName(NbtType type) noexcept {
    switch (type) {
    case NbtType::End:
        return "END";
    case NbtType::Byte:
        return "BYTE";
    case NbtType::Short:
        return "SHORT";
    case NbtType::Int:
        return "INT";
    case NbtType::Long:
        return "LONG";
    case NbtType::Float:
        return "FLOAT";
    case NbtType::Double:
        return "DOUBLE";
    case NbtType::ByteArray:
        return "BYTE_ARRAY";
    case NbtType::String:
        return "STRING";
    case NbtType::List:
        return "LIST";
    case NbtType::Compound:
        return "COMPOUND";
    case NbtType::IntArray:
        return "INT_ARRAY";
    case NbtType::LongArray:
        return "LONG_ARRAY";
    }
    return "END";
}

std::string_view nbtErrorMessage(NbtError error) noexcept {
    switch (error) {
    case NbtError::UnexpectedEnd:
        return "unexpected end of NBT data";
    case NbtError::UnknownTagId:
        return "unknown NBT tag id";
    case NbtError::NegativeSize:
        return "negative NBT array or list size";
    case NbtError::StringTooLong:
        return "NBT string too long";
    case NbtError::TruncatedUtf8:
        return "truncated modified UTF-8 sequence";
    case NbtError::InvalidUtf8:
        return "invalid modified UTF-8 leading byte";
    case NbtError::MixedListTypes:
        return "NBT list elements do not share one type";
    case NbtError::DepthLimitExceeded:
        return "NBT nesting depth limit exceeded";
    case NbtError::TrailingInput:
        return "trailing input after the NBT document";
    case NbtError::SyntaxError:
        return "SNBT syntax error";
    }
    return "unknown NBT error";
}

} // namespace basalt::wire::nbt
