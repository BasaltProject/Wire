#pragma once

#include <basalt/wire/nbt/NbtType.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace basalt::wire::nbt {

class NbtTag;

/// A homogeneous sequence of tags, as vanilla's TAG_List. An empty list keeps element type End,
/// which is what vanilla writes for it.
class NbtList {
public:
    NbtList();
    explicit NbtList(NbtType elementType);
    // No defaulted vector argument: instantiating it here would need NbtTag complete, and it is not
    // complete until further down this header.
    NbtList(NbtType elementType, std::vector<NbtTag> elements);
    NbtList(const NbtList&);
    NbtList(NbtList&&) noexcept;
    NbtList& operator=(const NbtList&);
    NbtList& operator=(NbtList&&) noexcept;
    ~NbtList();

    [[nodiscard]] NbtType elementType() const noexcept { return elementType_; }

    [[nodiscard]] std::size_t size() const noexcept;

    [[nodiscard]] bool empty() const noexcept;

    [[nodiscard]] const NbtTag& operator[](std::size_t index) const;

    [[nodiscard]] NbtTag& operator[](std::size_t index);

    [[nodiscard]] const std::vector<NbtTag>& elements() const noexcept { return elements_; }

    /// Appends a tag. The list adopts the element type of its first entry.
    bool push(NbtTag tag);

    // Out of line on purpose: the body would otherwise touch std::vector<NbtTag> while NbtTag is
    // still incomplete, which libc++ refuses.
    [[nodiscard]] std::vector<NbtTag>::const_iterator begin() const;

    [[nodiscard]] std::vector<NbtTag>::const_iterator end() const;

    friend bool operator==(const NbtList&, const NbtList&);

private:
    NbtType elementType_ = NbtType::End;
    std::vector<NbtTag> elements_;
};

/// A named set of tags in insertion order, as vanilla's TAG_Compound.
///
/// Names and values live in separate contiguous vectors rather than one map: a compound holds a
/// handful of entries, a lookup is a scan of the names alone, and the order has to survive because
/// it is the order the tag is written back out in.
class NbtCompound {
public:
    NbtCompound();
    NbtCompound(const NbtCompound&);
    NbtCompound(NbtCompound&&) noexcept;
    NbtCompound& operator=(const NbtCompound&);
    NbtCompound& operator=(NbtCompound&&) noexcept;
    ~NbtCompound();

    [[nodiscard]] std::size_t size() const noexcept;

    [[nodiscard]] bool empty() const noexcept;

    [[nodiscard]] const std::vector<std::string>& names() const noexcept { return names_; }

    [[nodiscard]] const std::vector<NbtTag>& values() const noexcept { return values_; }

    [[nodiscard]] bool contains(std::string_view key) const noexcept;

    [[nodiscard]] const NbtTag* find(std::string_view key) const noexcept;

    [[nodiscard]] NbtTag* find(std::string_view key) noexcept;

    void set(std::string_view key, NbtTag tag);

    bool remove(std::string_view key);

    void putByte(std::string_view key, std::int8_t value);
    void putBoolean(std::string_view key, bool value);
    void putShort(std::string_view key, std::int16_t value);
    void putInt(std::string_view key, std::int32_t value);
    void putLong(std::string_view key, std::int64_t value);
    void putFloat(std::string_view key, float value);
    void putDouble(std::string_view key, double value);
    void putString(std::string_view key, std::string value);
    void putByteArray(std::string_view key, std::vector<std::int8_t> value);
    void putIntArray(std::string_view key, std::vector<std::int32_t> value);
    void putLongArray(std::string_view key, std::vector<std::int64_t> value);

    [[nodiscard]] std::int8_t getByte(std::string_view key, std::int8_t fallback = 0) const noexcept;
    [[nodiscard]] bool getBoolean(std::string_view key, bool fallback = false) const noexcept;
    [[nodiscard]] std::int16_t getShort(std::string_view key, std::int16_t fallback = 0) const noexcept;
    [[nodiscard]] std::int32_t getInt(std::string_view key, std::int32_t fallback = 0) const noexcept;
    [[nodiscard]] std::int64_t getLong(std::string_view key, std::int64_t fallback = 0) const noexcept;
    [[nodiscard]] float getFloat(std::string_view key, float fallback = 0.0F) const noexcept;
    [[nodiscard]] double getDouble(std::string_view key, double fallback = 0.0) const noexcept;
    [[nodiscard]] std::string_view getString(std::string_view key, std::string_view fallback = "") const noexcept;
    [[nodiscard]] std::span<const std::int8_t> getByteArray(std::string_view key) const noexcept;
    [[nodiscard]] std::span<const std::int32_t> getIntArray(std::string_view key) const noexcept;
    [[nodiscard]] std::span<const std::int64_t> getLongArray(std::string_view key) const noexcept;
    [[nodiscard]] const NbtCompound* getCompound(std::string_view key) const noexcept;
    [[nodiscard]] const NbtList* getList(std::string_view key) const noexcept;

    friend bool operator==(const NbtCompound&, const NbtCompound&);

private:
    [[nodiscard]] std::size_t indexOf(std::string_view key) const noexcept;

    std::vector<std::string> names_;
    std::vector<NbtTag> values_;
};

/// Any NBT value. The alternatives are declared in tag id order, so the variant's index is the
/// wire type and no separate tag byte has to be kept in sync with the payload.
class NbtTag {
public:
    using Storage = std::variant<std::monostate, std::int8_t, std::int16_t, std::int32_t, std::int64_t, float, double,
                                 std::vector<std::int8_t>, std::string, NbtList, NbtCompound, std::vector<std::int32_t>,
                                 std::vector<std::int64_t>>;

    NbtTag() = default;

    [[nodiscard]] static NbtTag ofByte(std::int8_t value) { return NbtTag{Storage{value}}; }

    [[nodiscard]] static NbtTag ofBoolean(bool value) { return ofByte(value ? 1 : 0); }

    [[nodiscard]] static NbtTag ofShort(std::int16_t value) { return NbtTag{Storage{value}}; }

    [[nodiscard]] static NbtTag ofInt(std::int32_t value) { return NbtTag{Storage{value}}; }

    [[nodiscard]] static NbtTag ofLong(std::int64_t value) { return NbtTag{Storage{value}}; }

    [[nodiscard]] static NbtTag ofFloat(float value) { return NbtTag{Storage{value}}; }

    [[nodiscard]] static NbtTag ofDouble(double value) { return NbtTag{Storage{value}}; }

    [[nodiscard]] static NbtTag ofString(std::string value) { return NbtTag{Storage{std::move(value)}}; }

    [[nodiscard]] static NbtTag ofByteArray(std::vector<std::int8_t> value) {
        return NbtTag{Storage{std::move(value)}};
    }

    [[nodiscard]] static NbtTag ofIntArray(std::vector<std::int32_t> value) {
        return NbtTag{Storage{std::move(value)}};
    }

    [[nodiscard]] static NbtTag ofLongArray(std::vector<std::int64_t> value) {
        return NbtTag{Storage{std::move(value)}};
    }

    [[nodiscard]] static NbtTag ofList(NbtList value) { return NbtTag{Storage{std::move(value)}}; }

    [[nodiscard]] static NbtTag ofCompound(NbtCompound value) { return NbtTag{Storage{std::move(value)}}; }

    [[nodiscard]] NbtType type() const noexcept { return static_cast<NbtType>(storage_.index()); }

    template <typename T>
    [[nodiscard]] const T* get() const noexcept {
        return std::get_if<T>(&storage_);
    }

    template <typename T>
    [[nodiscard]] T* get() noexcept {
        return std::get_if<T>(&storage_);
    }

    [[nodiscard]] const Storage& storage() const noexcept { return storage_; }

    friend bool operator==(const NbtTag&, const NbtTag&) = default;

private:
    explicit NbtTag(Storage storage) : storage_(std::move(storage)) {}

    Storage storage_;
};

} // namespace basalt::wire::nbt
