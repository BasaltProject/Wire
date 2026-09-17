#include <basalt/wire/common/JavaNumbers.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <system_error>

namespace basalt::wire::nbt::snbt {
namespace {

constexpr bool isUnquotedChar(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
           c == '.' || c == '+';
}

constexpr bool isSpace(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

void appendQuotedString(std::string& out, std::string_view value) {
    out += '"';
    for (const char c : value) {
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        out += c;
    }
    out += '"';
}

void appendKey(std::string& out, std::string_view key) {
    const bool bare = !key.empty() && std::all_of(key.begin(), key.end(), isUnquotedChar);
    if (bare) {
        out += key;
    } else {
        appendQuotedString(out, key);
    }
}

template <typename T>
void appendNumber(std::string& out, T value) {
    std::array<char, 40> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    out.append(buffer.data(), result.ptr);
}

void appendTag(std::string& out, const NbtTag& tag);

void appendList(std::string& out, const NbtList& list) {
    out += '[';
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (i > 0) {
            out += ',';
        }
        appendTag(out, list[i]);
    }
    out += ']';
}

void appendTag(std::string& out, const NbtTag& tag) {
    switch (tag.type()) {
    case NbtType::End:
        break;
    case NbtType::Byte:
        appendNumber(out, static_cast<int>(*tag.get<std::int8_t>()));
        out += 'b';
        break;
    case NbtType::Short:
        appendNumber(out, *tag.get<std::int16_t>());
        out += 's';
        break;
    case NbtType::Int:
        appendNumber(out, *tag.get<std::int32_t>());
        break;
    case NbtType::Long:
        appendNumber(out, *tag.get<std::int64_t>());
        out += 'L';
        break;
    case NbtType::Float:
        // parity: `SnbtPrinterTagVisitor` appends the value itself, which is `Float.toString`.
        out += common::javaToString(*tag.get<float>());
        out += 'f';
        break;
    case NbtType::Double:
        out += common::javaToString(*tag.get<double>());
        out += 'd';
        break;
    case NbtType::String:
        appendQuotedString(out, *tag.get<std::string>());
        break;
    case NbtType::ByteArray: {
        out += "[B;";
        const auto& value = *tag.get<std::vector<std::int8_t>>();
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (i > 0) {
                out += ',';
            }
            appendNumber(out, static_cast<int>(value[i]));
            out += 'b';
        }
        out += ']';
        break;
    }
    case NbtType::IntArray: {
        out += "[I;";
        const auto& value = *tag.get<std::vector<std::int32_t>>();
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (i > 0) {
                out += ',';
            }
            appendNumber(out, value[i]);
        }
        out += ']';
        break;
    }
    case NbtType::LongArray: {
        out += "[L;";
        const auto& value = *tag.get<std::vector<std::int64_t>>();
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (i > 0) {
                out += ',';
            }
            appendNumber(out, value[i]);
            out += 'L';
        }
        out += ']';
        break;
    }
    case NbtType::List:
        appendList(out, *tag.get<NbtList>());
        break;
    case NbtType::Compound: {
        out += '{';
        const auto& compound = *tag.get<NbtCompound>();
        for (std::size_t i = 0; i < compound.size(); ++i) {
            if (i > 0) {
                out += ',';
            }
            appendKey(out, compound.names()[i]);
            out += ':';
            appendTag(out, compound.values()[i]);
        }
        out += '}';
        break;
    }
    }
}

class Parser {
public:
    Parser(std::string_view text, std::size_t start) : text_(text), position_(start) {}

    [[nodiscard]] std::size_t position() const noexcept { return position_; }

    [[nodiscard]] bool atEnd() const noexcept { return position_ >= text_.size(); }

    void skipWhitespace() noexcept {
        while (!atEnd() && isSpace(text_[position_])) {
            ++position_;
        }
    }

    std::expected<NbtTag, NbtError> parseValue(int depth);

private:
    [[nodiscard]] std::expected<char, NbtError> peek() const noexcept {
        if (atEnd()) {
            return std::unexpected{NbtError::UnexpectedEnd};
        }
        return text_[position_];
    }

    std::expected<void, NbtError> expect(char c) noexcept {
        if (atEnd() || text_[position_] != c) {
            return std::unexpected{NbtError::SyntaxError};
        }
        ++position_;
        return {};
    }

    std::expected<NbtTag, NbtError> parseCompound(int depth);
    std::expected<NbtTag, NbtError> parseListOrArray(int depth);
    std::expected<NbtTag, NbtError> parseTypedArray(char kind);
    std::expected<std::string, NbtError> parseQuotedString();
    std::expected<std::string_view, NbtError> parseUnquotedToken();

    std::string_view text_;
    std::size_t position_;
};

/// Vanilla's own SNBT accepts exponent notation, and `stringify` can emit it for large or small
/// values, so a token like `1e-7d` has to come back as a number and not as a string.
NbtTag numberOrKeyword(std::string_view token) {
    if (token == "true") {
        return NbtTag::ofBoolean(true);
    }
    if (token == "false") {
        return NbtTag::ofBoolean(false);
    }

    const char suffix = token.empty() ? '\0' : static_cast<char>(std::tolower(token.back()));
    const bool hasTypeSuffix =
        token.size() > 1 && (suffix == 'b' || suffix == 's' || suffix == 'l' || suffix == 'f' || suffix == 'd');
    const std::string_view body = hasTypeSuffix ? token.substr(0, token.size() - 1) : token;

    const auto parseInteger = [&body](auto& out) {
        const auto result = std::from_chars(body.data(), body.data() + body.size(), out);
        return result.ec == std::errc{} && result.ptr == body.data() + body.size();
    };
    const auto parseFloating = [&body](auto& out) {
        const auto result = std::from_chars(body.data(), body.data() + body.size(), out);
        return result.ec == std::errc{} && result.ptr == body.data() + body.size();
    };

    switch (hasTypeSuffix ? suffix : '\0') {
    case 'b': {
        std::int32_t value = 0;
        if (parseInteger(value) && value >= -128 && value <= 127) {
            return NbtTag::ofByte(static_cast<std::int8_t>(value));
        }
        break;
    }
    case 's': {
        std::int32_t value = 0;
        if (parseInteger(value) && value >= -32768 && value <= 32767) {
            return NbtTag::ofShort(static_cast<std::int16_t>(value));
        }
        break;
    }
    case 'l': {
        std::int64_t value = 0;
        if (parseInteger(value)) {
            return NbtTag::ofLong(value);
        }
        break;
    }
    case 'f': {
        float value = 0.0F;
        if (parseFloating(value)) {
            return NbtTag::ofFloat(value);
        }
        break;
    }
    case 'd': {
        double value = 0.0;
        if (parseFloating(value)) {
            return NbtTag::ofDouble(value);
        }
        break;
    }
    default: {
        std::int32_t asInt = 0;
        if (parseInteger(asInt)) {
            return NbtTag::ofInt(asInt);
        }
        double asDouble = 0.0;
        if (parseFloating(asDouble)) {
            return NbtTag::ofDouble(asDouble);
        }
        break;
    }
    }
    // Anything that does not parse as the number its suffix promises stays a string, which is what
    // an unquoted identifier in a command argument is.
    return NbtTag::ofString(std::string{token});
}

std::expected<std::string, NbtError> Parser::parseQuotedString() {
    const char quote = text_[position_];
    ++position_;
    std::string out;
    while (true) {
        if (atEnd()) {
            return std::unexpected{NbtError::UnexpectedEnd};
        }
        const char c = text_[position_++];
        if (c == '\\' && !atEnd()) {
            out += text_[position_++];
        } else if (c == quote) {
            return out;
        } else {
            out += c;
        }
    }
}

std::expected<std::string_view, NbtError> Parser::parseUnquotedToken() {
    const std::size_t start = position_;
    while (!atEnd() && isUnquotedChar(text_[position_])) {
        ++position_;
    }
    if (position_ == start) {
        return std::unexpected{NbtError::SyntaxError};
    }
    return text_.substr(start, position_ - start);
}

std::expected<NbtTag, NbtError> Parser::parseTypedArray(char kind) {
    std::vector<std::string_view> tokens;
    skipWhitespace();
    if (!atEnd() && text_[position_] == ']') {
        ++position_;
    } else {
        while (true) {
            skipWhitespace();
            auto token = parseUnquotedToken();
            if (!token) {
                return std::unexpected{token.error()};
            }
            tokens.push_back(*token);
            skipWhitespace();
            const auto next = peek();
            if (!next) {
                return std::unexpected{next.error()};
            }
            if (*next == ',') {
                ++position_;
            } else if (*next == ']') {
                ++position_;
                break;
            } else {
                return std::unexpected{NbtError::SyntaxError};
            }
        }
    }

    const auto stripSuffix = [](std::string_view token, char lower, char upper) {
        while (!token.empty() && (token.back() == lower || token.back() == upper)) {
            token.remove_suffix(1);
        }
        return token;
    };

    if (kind == 'B') {
        std::vector<std::int8_t> values;
        values.reserve(tokens.size());
        for (const std::string_view token : tokens) {
            const std::string_view body = stripSuffix(token, 'b', 'B');
            std::int32_t value = 0;
            if (std::from_chars(body.data(), body.data() + body.size(), value).ec != std::errc{}) {
                return std::unexpected{NbtError::SyntaxError};
            }
            values.push_back(static_cast<std::int8_t>(value));
        }
        return NbtTag::ofByteArray(std::move(values));
    }
    if (kind == 'I') {
        std::vector<std::int32_t> values;
        values.reserve(tokens.size());
        for (const std::string_view token : tokens) {
            std::int32_t value = 0;
            if (std::from_chars(token.data(), token.data() + token.size(), value).ec != std::errc{}) {
                return std::unexpected{NbtError::SyntaxError};
            }
            values.push_back(value);
        }
        return NbtTag::ofIntArray(std::move(values));
    }
    std::vector<std::int64_t> values;
    values.reserve(tokens.size());
    for (const std::string_view token : tokens) {
        const std::string_view body = stripSuffix(token, 'l', 'L');
        std::int64_t value = 0;
        if (std::from_chars(body.data(), body.data() + body.size(), value).ec != std::errc{}) {
            return std::unexpected{NbtError::SyntaxError};
        }
        values.push_back(value);
    }
    return NbtTag::ofLongArray(std::move(values));
}

std::expected<NbtTag, NbtError> Parser::parseListOrArray(int depth) {
    if (auto opened = expect('['); !opened) {
        return std::unexpected{opened.error()};
    }

    if (!atEnd() && position_ + 1 < text_.size() && text_[position_ + 1] == ';' &&
        (text_[position_] == 'B' || text_[position_] == 'I' || text_[position_] == 'L')) {
        const char kind = text_[position_];
        position_ += 2;
        return parseTypedArray(kind);
    }

    skipWhitespace();
    if (!atEnd() && text_[position_] == ']') {
        ++position_;
        return NbtTag::ofList(NbtList{NbtType::End});
    }

    std::vector<NbtTag> elements;
    while (true) {
        skipWhitespace();
        auto element = parseValue(depth + 1);
        if (!element) {
            return std::unexpected{element.error()};
        }
        if (!elements.empty() && element->type() != elements.front().type()) {
            return std::unexpected{NbtError::MixedListTypes};
        }
        elements.push_back(std::move(*element));

        skipWhitespace();
        const auto next = peek();
        if (!next) {
            return std::unexpected{next.error()};
        }
        if (*next == ',') {
            ++position_;
        } else if (*next == ']') {
            ++position_;
            const NbtType type = elements.front().type();
            return NbtTag::ofList(NbtList{type, std::move(elements)});
        } else {
            return std::unexpected{NbtError::SyntaxError};
        }
    }
}

std::expected<NbtTag, NbtError> Parser::parseCompound(int depth) {
    if (auto opened = expect('{'); !opened) {
        return std::unexpected{opened.error()};
    }

    NbtCompound compound;
    skipWhitespace();
    if (!atEnd() && text_[position_] == '}') {
        ++position_;
        return NbtTag::ofCompound(std::move(compound));
    }

    while (true) {
        skipWhitespace();
        const auto first = peek();
        if (!first) {
            return std::unexpected{first.error()};
        }

        std::string key;
        if (*first == '"' || *first == '\'') {
            auto quoted = parseQuotedString();
            if (!quoted) {
                return std::unexpected{quoted.error()};
            }
            key = std::move(*quoted);
        } else {
            auto bare = parseUnquotedToken();
            if (!bare) {
                return std::unexpected{bare.error()};
            }
            key = std::string{*bare};
        }

        skipWhitespace();
        if (auto colon = expect(':'); !colon) {
            return std::unexpected{colon.error()};
        }
        skipWhitespace();
        auto value = parseValue(depth + 1);
        if (!value) {
            return std::unexpected{value.error()};
        }
        compound.set(key, std::move(*value));

        skipWhitespace();
        const auto next = peek();
        if (!next) {
            return std::unexpected{next.error()};
        }
        if (*next == ',') {
            ++position_;
        } else if (*next == '}') {
            ++position_;
            return NbtTag::ofCompound(std::move(compound));
        } else {
            return std::unexpected{NbtError::SyntaxError};
        }
    }
}

std::expected<NbtTag, NbtError> Parser::parseValue(int depth) {
    if (depth > nbtIO::kMaxDepth) {
        return std::unexpected{NbtError::DepthLimitExceeded};
    }
    skipWhitespace();
    const auto first = peek();
    if (!first) {
        return std::unexpected{first.error()};
    }
    if (*first == '{') {
        return parseCompound(depth);
    }
    if (*first == '[') {
        return parseListOrArray(depth);
    }
    if (*first == '"' || *first == '\'') {
        return parseQuotedString().transform([](std::string v) { return NbtTag::ofString(std::move(v)); });
    }
    return parseUnquotedToken().transform(numberOrKeyword);
}

} // namespace

std::string stringify(const NbtTag& tag) {
    std::string out;
    appendTag(out, tag);
    return out;
}

std::expected<NbtTag, NbtError> parse(std::string_view text) {
    Parser parser{text, 0};
    parser.skipWhitespace();
    auto tag = parser.parseValue(0);
    if (!tag) {
        return tag;
    }
    parser.skipWhitespace();
    if (!parser.atEnd()) {
        return std::unexpected{NbtError::TrailingInput};
    }
    return tag;
}

std::expected<SnbtValue, NbtError> parseFrom(std::string_view text, std::size_t from) {
    Parser parser{text, from};
    parser.skipWhitespace();
    auto tag = parser.parseValue(0);
    if (!tag) {
        return std::unexpected{tag.error()};
    }
    return SnbtValue{std::move(*tag), parser.position()};
}

} // namespace basalt::wire::nbt::snbt
