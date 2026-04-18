#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace basalt::wire::io::detail {

class ByteStorage {
    using Regular = std::vector<std::uint8_t>;
    using Managed = std::pmr::vector<std::uint8_t>;

public:
    ByteStorage() = default;

    explicit ByteStorage(std::pmr::memory_resource& resource) : bytes_(std::in_place_type<Managed>, &resource) {}

    ByteStorage(const ByteStorage& other) {
        if (const auto* regular = std::get_if<Regular>(&other.bytes_))
            bytes_ = *regular;
        else {
            const auto& managed = std::get<Managed>(other.bytes_);
            bytes_.emplace<Managed>(managed, managed.get_allocator());
        }
    }

    ByteStorage(ByteStorage&&) noexcept = default;

    ByteStorage& operator=(const ByteStorage& other) {
        if (this == &other)
            return *this;
        if (auto* managed = std::get_if<Managed>(&bytes_))
            managed->assign(other.data(), other.data() + other.size());
        else if (const auto* source = std::get_if<Managed>(&other.bytes_)) {
            Managed copy{*source, source->get_allocator()};
            bytes_.emplace<Managed>(std::move(copy));
        } else
            bytes_ = std::get<Regular>(other.bytes_);
        return *this;
    }

    ByteStorage& operator=(ByteStorage&& other) {
        if (this == &other)
            return *this;
        if (auto* managed = std::get_if<Managed>(&bytes_)) {
            if (auto* source = std::get_if<Managed>(&other.bytes_))
                *managed = std::move(*source);
            else {
                managed->assign(other.data(), other.data() + other.size());
                std::get<Regular>(other.bytes_).clear();
            }
        } else
            bytes_ = std::move(other.bytes_);
        return *this;
    }

    void adopt(Regular bytes) { bytes_.emplace<Regular>(std::move(bytes)); }

    [[nodiscard]] std::size_t size() const noexcept {
        if (const auto* regular = std::get_if<Regular>(&bytes_))
            return regular->size();
        return std::get<Managed>(bytes_).size();
    }

    [[nodiscard]] std::size_t capacity() const noexcept {
        if (const auto* regular = std::get_if<Regular>(&bytes_))
            return regular->capacity();
        return std::get<Managed>(bytes_).capacity();
    }

    [[nodiscard]] const std::uint8_t* data() const noexcept {
        if (const auto* regular = std::get_if<Regular>(&bytes_))
            return regular->data();
        return std::get<Managed>(bytes_).data();
    }

    [[nodiscard]] std::uint8_t* data() noexcept {
        if (auto* regular = std::get_if<Regular>(&bytes_))
            return regular->data();
        return std::get<Managed>(bytes_).data();
    }

    [[nodiscard]] std::uint8_t operator[](std::size_t index) const noexcept { return data()[index]; }

    void reserve(std::size_t capacity) {
        if (auto* regular = std::get_if<Regular>(&bytes_))
            regular->reserve(capacity);
        else
            std::get<Managed>(bytes_).reserve(capacity);
    }

    void resize(std::size_t size) {
        if (auto* regular = std::get_if<Regular>(&bytes_))
            regular->resize(size);
        else
            std::get<Managed>(bytes_).resize(size);
    }

    void push_back(std::uint8_t value) {
        if (auto* regular = std::get_if<Regular>(&bytes_))
            regular->push_back(value);
        else
            std::get<Managed>(bytes_).push_back(value);
    }

    void append(std::span<const std::uint8_t> bytes) {
        if (bytes.empty())
            return;
        if (auto* regular = std::get_if<Regular>(&bytes_))
            regular->insert(regular->end(), bytes.begin(), bytes.end());
        else {
            auto& managed = std::get<Managed>(bytes_);
            managed.insert(managed.end(), bytes.begin(), bytes.end());
        }
    }

    void clear() noexcept {
        if (auto* regular = std::get_if<Regular>(&bytes_))
            regular->clear();
        else
            std::get<Managed>(bytes_).clear();
    }

private:
    std::variant<Regular, Managed> bytes_;
};

} // namespace basalt::wire::io::detail
