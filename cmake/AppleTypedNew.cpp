#include <cstddef>
#include <new>

namespace std {
enum class __type_descriptor_t : unsigned long long;
}

void* operator new(std::size_t size, std::__type_descriptor_t) { return ::operator new(size); }

void* operator new(std::size_t size, const std::nothrow_t& tag, std::__type_descriptor_t) noexcept {
    return ::operator new(size, tag);
}

void* operator new[](std::size_t size, std::__type_descriptor_t) { return ::operator new[](size); }

void* operator new[](std::size_t size, const std::nothrow_t& tag, std::__type_descriptor_t) noexcept {
    return ::operator new[](size, tag);
}

void operator delete(void* pointer, std::__type_descriptor_t) noexcept { ::operator delete(pointer); }

void operator delete(void* pointer, const std::nothrow_t& tag, std::__type_descriptor_t) noexcept {
    ::operator delete(pointer, tag);
}

void operator delete[](void* pointer, std::__type_descriptor_t) noexcept { ::operator delete[](pointer); }

void operator delete[](void* pointer, const std::nothrow_t& tag, std::__type_descriptor_t) noexcept {
    ::operator delete[](pointer, tag);
}

void operator delete(void* pointer, std::size_t size, std::__type_descriptor_t) noexcept {
    ::operator delete(pointer, size);
}

void operator delete[](void* pointer, std::size_t size, std::__type_descriptor_t) noexcept {
    ::operator delete[](pointer, size);
}

void* operator new(std::size_t size, std::align_val_t alignment, std::__type_descriptor_t) {
    return ::operator new(size, alignment);
}

void* operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t& tag,
                   std::__type_descriptor_t) noexcept {
    return ::operator new(size, alignment, tag);
}

void* operator new[](std::size_t size, std::align_val_t alignment, std::__type_descriptor_t) {
    return ::operator new[](size, alignment);
}

void* operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t& tag,
                     std::__type_descriptor_t) noexcept {
    return ::operator new[](size, alignment, tag);
}

void operator delete(void* pointer, std::align_val_t alignment, std::__type_descriptor_t) noexcept {
    ::operator delete(pointer, alignment);
}

void operator delete(void* pointer, std::align_val_t alignment, const std::nothrow_t& tag,
                     std::__type_descriptor_t) noexcept {
    ::operator delete(pointer, alignment, tag);
}

void operator delete[](void* pointer, std::align_val_t alignment, std::__type_descriptor_t) noexcept {
    ::operator delete[](pointer, alignment);
}

void operator delete[](void* pointer, std::align_val_t alignment, const std::nothrow_t& tag,
                       std::__type_descriptor_t) noexcept {
    ::operator delete[](pointer, alignment, tag);
}

void operator delete(void* pointer, std::size_t size, std::align_val_t alignment, std::__type_descriptor_t) noexcept {
    ::operator delete(pointer, size, alignment);
}

void operator delete[](void* pointer, std::size_t size, std::align_val_t alignment, std::__type_descriptor_t) noexcept {
    ::operator delete[](pointer, size, alignment);
}
