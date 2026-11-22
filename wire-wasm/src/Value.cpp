#include <basalt/wire/wasm/Value.hpp>

#include <bit>
#include <cstring>

namespace basalt::wire::wasm {
namespace {
enum Tag : std::uint8_t {
    kNull = 0x00,
    kFalse = 0x01,
    kTrue = 0x02,
    kInt = 0x03,
    kFloat = 0x04,
    kString = 0x05,
    kBytes = 0x06,
    kList = 0x07,
    kMap = 0x08,
};

void putVarint(std::string& out, std::uint64_t value) {
    while (value >= 0x80U) {
        out.push_back(static_cast<char>((value & 0x7FU) | 0x80U));
        value >>= 7U;
    }
    out.push_back(static_cast<char>(value));
}

struct Encoder {
    std::string& out;
    std::size_t nodes = 0;
    bool ok = true;

    void blob(std::string_view bytes) {
        putVarint(out, bytes.size());
        out.append(bytes);
    }

    void write(const Value& value, std::size_t depth) {
        if (!ok || ++nodes > kValueMaxNodes || depth > kValueMaxDepth || out.size() > kValueMaxBytes) {
            ok = false;
            return;
        }

        struct Visitor {
            Encoder& self;
            std::size_t depth;

            void operator()(std::monostate) const { self.out.push_back(static_cast<char>(kNull)); }

            void operator()(bool v) const { self.out.push_back(static_cast<char>(v ? kTrue : kFalse)); }

            void operator()(std::int64_t v) const {
                self.out.push_back(static_cast<char>(kInt));
                putVarint(self.out, (static_cast<std::uint64_t>(v) << 1U) ^ static_cast<std::uint64_t>(v >> 63));
            }

            void operator()(double v) const {
                self.out.push_back(static_cast<char>(kFloat));
                const auto bits = std::bit_cast<std::uint64_t>(v);
                for (unsigned shift = 0; shift < 64; shift += 8) {
                    self.out.push_back(static_cast<char>((bits >> shift) & 0xFFU));
                }
            }

            void operator()(const std::string& v) const {
                self.out.push_back(static_cast<char>(kString));
                self.blob(v);
            }

            void operator()(const Blob& v) const {
                self.out.push_back(static_cast<char>(kBytes));
                self.blob({reinterpret_cast<const char*>(v.data.data()), v.data.size()});
            }

            void operator()(const Value::List& v) const {
                self.out.push_back(static_cast<char>(kList));
                putVarint(self.out, v.size());
                for (const Value& item : v) {
                    self.write(item, depth + 1);
                }
            }

            void operator()(const Value::Map& v) const {
                self.out.push_back(static_cast<char>(kMap));
                putVarint(self.out, v.size());
                for (const MapEntry& entry : v) {
                    if (entry.key.size() > kValueMaxKeyBytes) {
                        self.ok = false;
                        return;
                    }
                    self.blob(entry.key);
                    self.write(entry.value, depth + 1);
                }
            }
        };

        std::visit(Visitor{*this, depth}, value.data);
    }
};

class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    [[nodiscard]] std::expected<Value, std::string> document() {
        if (bytes_.size() > kValueMaxBytes) {
            return std::unexpected("the document is over the size limit");
        }
        auto value = read(0);
        if (value && position_ != bytes_.size()) {
            return std::unexpected("bytes after the value");
        }
        return value;
    }

private:
    using Fail = std::unexpected<std::string>;

    [[nodiscard]] std::size_t remaining() const { return bytes_.size() - position_; }

    [[nodiscard]] bool varint(std::uint64_t& out) {
        out = 0;
        for (unsigned shift = 0; shift < 70; shift += 7) {
            if (position_ >= bytes_.size()) {
                return false;
            }
            const std::uint8_t byte = bytes_[position_++];
            if (shift == 63 && byte > 1U) {
                return false;
            }
            out |= static_cast<std::uint64_t>(byte & 0x7FU) << shift;
            if ((byte & 0x80U) == 0) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool take(std::size_t length, std::string_view& out) {
        if (length > remaining()) {
            return false;
        }
        out = {reinterpret_cast<const char*>(bytes_.data() + position_), length};
        position_ += length;
        return true;
    }

    [[nodiscard]] std::expected<Value, std::string> read(std::size_t depth) {
        if (depth > kValueMaxDepth) {
            return Fail("nesting is over the depth limit");
        }
        if (++nodes_ > kValueMaxNodes) {
            return Fail("the document has too many values");
        }
        if (remaining() == 0) {
            return Fail("truncated value");
        }
        std::uint64_t number = 0;
        std::string_view text;
        switch (bytes_[position_++]) {
        case kNull:
            return Value{};
        case kFalse:
            return Value{false};
        case kTrue:
            return Value{true};
        case kInt:
            if (!varint(number)) {
                return Fail("bad integer");
            }
            return Value{static_cast<std::int64_t>((number >> 1U) ^ (~(number & 1U) + 1U))};
        case kFloat: {
            if (remaining() < 8) {
                return Fail("truncated float");
            }
            std::uint64_t bits = 0;
            std::memcpy(&bits, bytes_.data() + position_, 8);
            position_ += 8;
            if constexpr (std::endian::native == std::endian::big) {
                bits = std::byteswap(bits);
            }
            return Value{std::bit_cast<double>(bits)};
        }
        case kString:
        case kBytes: {
            const bool isString = bytes_[position_ - 1] == kString;
            if (!varint(number) || number > remaining() || !take(static_cast<std::size_t>(number), text)) {
                return Fail("truncated string or bytes");
            }
            if (isString) {
                return Value{std::string{text}};
            }
            return Value{Blob{std::vector<std::uint8_t>(text.begin(), text.end())}};
        }
        case kList: {
            if (!varint(number) || number > remaining()) {
                return Fail("bad list count");
            }
            Value::List items;
            items.reserve(static_cast<std::size_t>(number));
            for (std::uint64_t index = 0; index < number; ++index) {
                auto item = read(depth + 1);
                if (!item) {
                    return item;
                }
                items.push_back(std::move(*item));
            }
            return Value{std::move(items)};
        }
        case kMap: {
            if (!varint(number) || number > remaining()) {
                return Fail("bad map count");
            }
            Value::Map entries;
            entries.reserve(static_cast<std::size_t>(number));
            for (std::uint64_t index = 0; index < number; ++index) {
                std::uint64_t length = 0;
                if (!varint(length) || length > kValueMaxKeyBytes || !take(static_cast<std::size_t>(length), text)) {
                    return Fail("bad map key");
                }
                for (const MapEntry& seen : entries) {
                    if (seen.key == text) {
                        return Fail("duplicate map key");
                    }
                }
                std::string key{text};
                auto item = read(depth + 1);
                if (!item) {
                    return item;
                }
                entries.push_back(MapEntry{std::move(key), std::move(*item)});
            }
            return Value{std::move(entries)};
        }
        default:
            return Fail("unknown value tag");
        }
    }

    std::span<const std::uint8_t> bytes_;
    std::size_t position_ = 0;
    std::size_t nodes_ = 0;
};
} // namespace

const Value* Value::find(std::string_view key) const noexcept {
    if (const Map* map = asMap()) {
        for (const MapEntry& entry : *map) {
            if (entry.key == key) {
                return &entry.value;
            }
        }
    }
    return nullptr;
}

bool encodeValue(const Value& value, std::string& out) {
    const std::size_t before = out.size();
    Encoder encoder{out};
    encoder.write(value, 0);
    if (!encoder.ok || out.size() - before > kValueMaxBytes) {
        out.resize(before);
        return false;
    }
    return true;
}

std::expected<Value, std::string> decodeValue(std::span<const std::uint8_t> bytes) { return Reader{bytes}.document(); }

} // namespace basalt::wire::wasm
