#include <basalt/wire/nbt/NbtTag.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::wire::nbt {

NbtList::NbtList() = default;

NbtList::NbtList(NbtType elementType) : elementType_(elementType) {}

NbtList::NbtList(NbtType elementType, std::vector<NbtTag> elements)
    : elementType_(elementType), elements_(std::move(elements)) {}

NbtList::NbtList(const NbtList&) = default;

NbtList::NbtList(NbtList&&) noexcept = default;

NbtList& NbtList::operator=(const NbtList&) = default;

NbtList& NbtList::operator=(NbtList&&) noexcept = default;

NbtList::~NbtList() = default;

std::size_t NbtList::size() const noexcept { return elements_.size(); }

bool NbtList::empty() const noexcept { return elements_.empty(); }

const NbtTag& NbtList::operator[](std::size_t index) const { return elements_[index]; }

NbtTag& NbtList::operator[](std::size_t index) { return elements_[index]; }

std::vector<NbtTag>::const_iterator NbtList::begin() const { return elements_.begin(); }

std::vector<NbtTag>::const_iterator NbtList::end() const { return elements_.end(); }

bool NbtList::push(NbtTag tag) {
    if (elements_.empty()) {
        elementType_ = tag.type();
    } else if (tag.type() != elementType_) {
        return false;
    }
    elements_.push_back(std::move(tag));
    return true;
}

bool operator==(const NbtList& lhs, const NbtList& rhs) {
    return lhs.elementType_ == rhs.elementType_ && lhs.elements_ == rhs.elements_;
}

NbtCompound::NbtCompound() = default;

NbtCompound::NbtCompound(const NbtCompound&) = default;

NbtCompound::NbtCompound(NbtCompound&&) noexcept = default;

NbtCompound& NbtCompound::operator=(const NbtCompound&) = default;

NbtCompound& NbtCompound::operator=(NbtCompound&&) noexcept = default;

NbtCompound::~NbtCompound() = default;

std::size_t NbtCompound::size() const noexcept { return names_.size(); }

bool NbtCompound::empty() const noexcept { return names_.empty(); }

std::size_t NbtCompound::indexOf(std::string_view key) const noexcept {
    for (std::size_t i = 0; i < names_.size(); ++i) {
        if (names_[i] == key) {
            return i;
        }
    }
    return names_.size();
}

bool NbtCompound::contains(std::string_view key) const noexcept { return indexOf(key) != names_.size(); }

const NbtTag* NbtCompound::find(std::string_view key) const noexcept {
    const std::size_t at = indexOf(key);
    return at == names_.size() ? nullptr : &values_[at];
}

NbtTag* NbtCompound::find(std::string_view key) noexcept {
    const std::size_t at = indexOf(key);
    return at == names_.size() ? nullptr : &values_[at];
}

void NbtCompound::set(std::string_view key, NbtTag tag) {
    const std::size_t at = indexOf(key);
    if (at == names_.size()) {
        names_.emplace_back(key);
        values_.push_back(std::move(tag));
    } else {
        values_[at] = std::move(tag);
    }
}

bool NbtCompound::remove(std::string_view key) {
    const std::size_t at = indexOf(key);
    if (at == names_.size()) {
        return false;
    }
    names_.erase(names_.begin() + static_cast<std::ptrdiff_t>(at));
    values_.erase(values_.begin() + static_cast<std::ptrdiff_t>(at));
    return true;
}

void NbtCompound::putByte(std::string_view key, std::int8_t value) { set(key, NbtTag::ofByte(value)); }

void NbtCompound::putBoolean(std::string_view key, bool value) { set(key, NbtTag::ofBoolean(value)); }

void NbtCompound::putShort(std::string_view key, std::int16_t value) { set(key, NbtTag::ofShort(value)); }

void NbtCompound::putInt(std::string_view key, std::int32_t value) { set(key, NbtTag::ofInt(value)); }

void NbtCompound::putLong(std::string_view key, std::int64_t value) { set(key, NbtTag::ofLong(value)); }

void NbtCompound::putFloat(std::string_view key, float value) { set(key, NbtTag::ofFloat(value)); }

void NbtCompound::putDouble(std::string_view key, double value) { set(key, NbtTag::ofDouble(value)); }

void NbtCompound::putString(std::string_view key, std::string value) { set(key, NbtTag::ofString(std::move(value))); }

void NbtCompound::putByteArray(std::string_view key, std::vector<std::int8_t> value) {
    set(key, NbtTag::ofByteArray(std::move(value)));
}

void NbtCompound::putIntArray(std::string_view key, std::vector<std::int32_t> value) {
    set(key, NbtTag::ofIntArray(std::move(value)));
}

void NbtCompound::putLongArray(std::string_view key, std::vector<std::int64_t> value) {
    set(key, NbtTag::ofLongArray(std::move(value)));
}

namespace {

template <typename T>
const T* payloadOf(const NbtCompound& compound, std::string_view key) noexcept {
    const NbtTag* tag = compound.find(key);
    return tag != nullptr ? tag->get<T>() : nullptr;
}

} // namespace

std::int8_t NbtCompound::getByte(std::string_view key, std::int8_t fallback) const noexcept {
    const auto* value = payloadOf<std::int8_t>(*this, key);
    return value != nullptr ? *value : fallback;
}

bool NbtCompound::getBoolean(std::string_view key, bool fallback) const noexcept {
    const auto* value = payloadOf<std::int8_t>(*this, key);
    return value != nullptr ? *value != 0 : fallback;
}

std::int16_t NbtCompound::getShort(std::string_view key, std::int16_t fallback) const noexcept {
    const auto* value = payloadOf<std::int16_t>(*this, key);
    return value != nullptr ? *value : fallback;
}

std::int32_t NbtCompound::getInt(std::string_view key, std::int32_t fallback) const noexcept {
    const auto* value = payloadOf<std::int32_t>(*this, key);
    return value != nullptr ? *value : fallback;
}

std::int64_t NbtCompound::getLong(std::string_view key, std::int64_t fallback) const noexcept {
    const auto* value = payloadOf<std::int64_t>(*this, key);
    return value != nullptr ? *value : fallback;
}

float NbtCompound::getFloat(std::string_view key, float fallback) const noexcept {
    const auto* value = payloadOf<float>(*this, key);
    return value != nullptr ? *value : fallback;
}

double NbtCompound::getDouble(std::string_view key, double fallback) const noexcept {
    const auto* value = payloadOf<double>(*this, key);
    return value != nullptr ? *value : fallback;
}

std::string_view NbtCompound::getString(std::string_view key, std::string_view fallback) const noexcept {
    const auto* value = payloadOf<std::string>(*this, key);
    return value != nullptr ? std::string_view{*value} : fallback;
}

std::span<const std::int8_t> NbtCompound::getByteArray(std::string_view key) const noexcept {
    const auto* value = payloadOf<std::vector<std::int8_t>>(*this, key);
    return value != nullptr ? std::span<const std::int8_t>{*value} : std::span<const std::int8_t>{};
}

std::span<const std::int32_t> NbtCompound::getIntArray(std::string_view key) const noexcept {
    const auto* value = payloadOf<std::vector<std::int32_t>>(*this, key);
    return value != nullptr ? std::span<const std::int32_t>{*value} : std::span<const std::int32_t>{};
}

std::span<const std::int64_t> NbtCompound::getLongArray(std::string_view key) const noexcept {
    const auto* value = payloadOf<std::vector<std::int64_t>>(*this, key);
    return value != nullptr ? std::span<const std::int64_t>{*value} : std::span<const std::int64_t>{};
}

const NbtCompound* NbtCompound::getCompound(std::string_view key) const noexcept {
    return payloadOf<NbtCompound>(*this, key);
}

const NbtList* NbtCompound::getList(std::string_view key) const noexcept { return payloadOf<NbtList>(*this, key); }

bool operator==(const NbtCompound& lhs, const NbtCompound& rhs) {
    if (lhs.names_.size() != rhs.names_.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.names_.size(); ++i) {
        const NbtTag* other = rhs.find(lhs.names_[i]);
        if (other == nullptr || !(*other == lhs.values_[i])) {
            return false;
        }
    }
    return true;
}

} // namespace basalt::wire::nbt
