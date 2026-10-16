#include <basalt/wire/io/EventLoop.hpp>
#include <basalt/wire/io/TcpSocket.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <unordered_map>

#if defined(_WIN32)
#include <winsock2.h>
using pollfd_type = WSAPOLLFD;
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
using pollfd_type = pollfd;
#if (defined(__linux__) && !defined(BASALT_IO_FORCE_POLL))
#include <sys/epoll.h>
#elif (defined(__APPLE__) && !defined(BASALT_IO_FORCE_POLL))
#include <sys/event.h>
#endif
#endif

namespace basalt::wire::io {
namespace {
#if defined(POLLRDHUP)
// Reported even when reads are paused, like EPOLLRDHUP; WSAPoll has no equivalent.
constexpr short kPeerClosedEvent = POLLRDHUP;
#else
constexpr short kPeerClosedEvent = 0;
#endif
} // namespace
namespace {
int lastError() {
#if defined(_WIN32)
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool interrupted(int error) {
#if defined(_WIN32)
    return error == WSAEINTR;
#else
    return error == EINTR;
#endif
}

bool wouldBlock(int error) {
#if defined(_WIN32)
    return error == WSAEWOULDBLOCK;
#else
    return error == EAGAIN || error == EWOULDBLOCK;
#endif
}

int remaining(std::optional<std::chrono::steady_clock::time_point> deadline) {
    if (!deadline) {
        return -1;
    }
    const auto duration = std::chrono::ceil<std::chrono::milliseconds>(*deadline - std::chrono::steady_clock::now());
    return static_cast<int>(std::clamp<std::int64_t>(duration.count(), 0, std::numeric_limits<int>::max()));
}
} // namespace

struct EventLoop::Impl {
    struct Registration {
        std::int64_t handle;
        IoInterest interest;
        std::uint64_t token;
        std::uint64_t serial;
    };

    std::unordered_map<std::int64_t, Registration> watched;
    std::unordered_map<std::uint64_t, std::int64_t> handles;
    std::vector<IoReady> ready;
    std::vector<pollfd_type> pollEntries;
    std::vector<std::uint64_t> pollSerials;
    std::uint64_t nextSerial = 1;
    std::int64_t wakeReader = -1;
    std::int64_t wakeWriter = -1;
    int error = 0;
#if defined(_WIN32)
    TcpConnection reader;
    TcpConnection writer;
#elif (defined(__linux__) && !defined(BASALT_IO_FORCE_POLL)) || (defined(__APPLE__) && !defined(BASALT_IO_FORCE_POLL))
    int queue = -1;
#endif

    Impl() {
#if defined(_WIN32)
        const auto address = TcpAddress::literal("127.0.0.1", 0);
        if (!address) {
            error = address.error().platformCode;
            return;
        }
        auto listener = TcpListener::bind(*address);
        if (!listener) {
            error = listener.error().platformCode;
            return;
        }
        const auto port = listener->localPort();
        if (!port) {
            error = port.error().platformCode;
            return;
        }
        auto client = TcpConnection::connect("127.0.0.1", *port);
        if (!client) {
            error = client.error().platformCode;
            return;
        }
        auto accepted = listener->accept();
        if (!accepted) {
            error = accepted.error().platformCode;
            return;
        }
        writer = std::move(*client);
        reader = std::move(*accepted);
        const auto readMode = reader.setNonBlocking();
        const auto writeMode = writer.setNonBlocking();
        if (!readMode || !writeMode) {
            error = !readMode ? readMode.error().platformCode : writeMode.error().platformCode;
            return;
        }
        wakeReader = reader.nativeHandle();
        wakeWriter = writer.nativeHandle();
#else
        int pair[2];
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, pair) != 0) {
            error = errno;
            return;
        }
        wakeReader = pair[0];
        wakeWriter = pair[1];
        for (const int handle : pair) {
            if (::fcntl(handle, F_SETFL, O_NONBLOCK) != 0 || ::fcntl(handle, F_SETFD, FD_CLOEXEC) != 0) {
                error = errno;
                return;
            }
        }
#if (defined(__linux__) && !defined(BASALT_IO_FORCE_POLL))
        queue = ::epoll_create1(EPOLL_CLOEXEC);
        if (queue < 0) {
            error = errno;
            return;
        }
#elif (defined(__APPLE__) && !defined(BASALT_IO_FORCE_POLL))
        queue = ::kqueue();
        if (queue < 0) {
            error = errno;
            return;
        }
#endif
#endif
        const auto result = update(Registration{wakeReader, {true, false}, 0, 0}, false);
        if (!result) {
            error = result.error();
        }
    }

    ~Impl() {
#if !defined(_WIN32)
        if (wakeReader >= 0) {
            ::close(static_cast<int>(wakeReader));
        }
        if (wakeWriter >= 0) {
            ::close(static_cast<int>(wakeWriter));
        }
#if (defined(__linux__) && !defined(BASALT_IO_FORCE_POLL)) || (defined(__APPLE__) && !defined(BASALT_IO_FORCE_POLL))
        if (queue >= 0) {
            ::close(queue);
        }
#endif
#endif
    }

    std::expected<void, int> update(const Registration& item, bool exists) {
        static_cast<void>(exists);
#if (defined(__linux__) && !defined(BASALT_IO_FORCE_POLL))
        epoll_event event{};
        event.events = (item.interest.readable ? EPOLLIN : 0U) | (item.interest.writable ? EPOLLOUT : 0U) | EPOLLRDHUP;
        event.data.u64 = item.serial;
        if (::epoll_ctl(queue, exists ? EPOLL_CTL_MOD : EPOLL_CTL_ADD, static_cast<int>(item.handle), &event) != 0) {
            return std::unexpected{errno};
        }
#elif (defined(__APPLE__) && !defined(BASALT_IO_FORCE_POLL))
        std::array<struct kevent, 2> changes{};
        const auto data = reinterpret_cast<void*>(static_cast<std::uintptr_t>(item.serial));
        EV_SET(&changes[0], static_cast<std::uintptr_t>(item.handle), EVFILT_READ,
               EV_ADD | (item.interest.readable ? EV_ENABLE : EV_DISABLE), 0, 0, data);
        EV_SET(&changes[1], static_cast<std::uintptr_t>(item.handle), EVFILT_WRITE,
               EV_ADD | (item.interest.writable ? EV_ENABLE : EV_DISABLE), 0, 0, data);
        if (::kevent(queue, changes.data(), 2, nullptr, 0, nullptr) < 0) {
            return std::unexpected{errno};
        }
#else
        static_cast<void>(item);
        static_cast<void>(exists);
#endif
        return {};
    }

    void drainWake() {
        std::array<char, 128> bytes{};
        while (true) {
#if defined(_WIN32)
            const int got = ::recv(static_cast<SOCKET>(wakeReader), bytes.data(), static_cast<int>(bytes.size()), 0);
#else
            const auto got = ::recv(static_cast<int>(wakeReader), bytes.data(), bytes.size(), 0);
#endif
            if (got > 0) {
                continue;
            }
            if (got < 0 && interrupted(lastError())) {
                continue;
            }
            return;
        }
    }

    void emit(std::uint64_t serial, bool readable, bool writable, bool closed) {
        if (serial == 0) {
            drainWake();
            return;
        }
        const auto found = handles.find(serial);
        if (found == handles.end()) {
            return;
        }
        const Registration& item = watched.at(found->second);
        const auto existing =
            std::ranges::find_if(ready, [&](const IoReady& event) { return event.handle == item.handle; });
        if (existing != ready.end()) {
            existing->readable |= readable;
            existing->writable |= writable;
            existing->closed |= closed;
        } else {
            ready.push_back({item.handle, item.token, readable, writable, closed});
        }
    }
};

EventLoop::EventLoop() : impl_(std::make_unique<Impl>()) {}

EventLoop::~EventLoop() = default;

std::size_t EventLoop::watchedCount() const noexcept { return impl_->watched.size(); }

std::expected<void, int> EventLoop::watch(std::int64_t handle, IoInterest interest, std::uint64_t token) {
    if (impl_->error != 0) {
        return std::unexpected{impl_->error};
    }
    if (handle < 0) {
#if defined(_WIN32)
        return std::unexpected{WSAEINVAL};
#else
        return std::unexpected{EINVAL};
#endif
    }
    const auto old = impl_->watched.find(handle);
    if (old != impl_->watched.end() && old->second.token == token &&
        old->second.interest.readable == interest.readable && old->second.interest.writable == interest.writable)
        return {};
    const Impl::Registration item{handle, interest, token, impl_->nextSerial++};
    const auto result = impl_->update(item, old != impl_->watched.end());
    if (!result) {
        return result;
    }
    if (old != impl_->watched.end()) {
        impl_->handles.erase(old->second.serial);
    }
    impl_->watched.insert_or_assign(handle, item);
    impl_->handles.emplace(item.serial, handle);
    impl_->ready.reserve(impl_->watched.size());
    impl_->pollEntries.reserve(impl_->watched.size() + 1);
    impl_->pollSerials.reserve(impl_->watched.size() + 1);
    return {};
}

void EventLoop::watch(std::int64_t handle) {
    if (handle < 0 || impl_->watched.contains(handle)) {
        return;
    }
    static_cast<void>(watch(handle, {true, false}, 0));
}

void EventLoop::unwatch(std::int64_t handle) {
    const auto item = impl_->watched.find(handle);
    if (item == impl_->watched.end()) {
        return;
    }
#if (defined(__linux__) && !defined(BASALT_IO_FORCE_POLL))
    ::epoll_ctl(impl_->queue, EPOLL_CTL_DEL, static_cast<int>(handle), nullptr);
#elif (defined(__APPLE__) && !defined(BASALT_IO_FORCE_POLL))
    std::array<struct kevent, 2> changes{};
    EV_SET(&changes[0], static_cast<std::uintptr_t>(handle), EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    EV_SET(&changes[1], static_cast<std::uintptr_t>(handle), EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    ::kevent(impl_->queue, changes.data(), 2, nullptr, 0, nullptr);
#endif
    impl_->handles.erase(item->second.serial);
    impl_->watched.erase(item);
}

std::expected<std::span<const IoReady>, int> EventLoop::wait(std::optional<int> timeoutMillis) {
    if (impl_->error != 0) {
        return std::unexpected{impl_->error};
    }
    impl_->ready.clear();
    const auto deadline =
        timeoutMillis
            ? std::optional{std::chrono::steady_clock::now() + std::chrono::milliseconds{std::max(0, *timeoutMillis)}}
            : std::nullopt;
    while (true) {
        const int timeout = remaining(deadline);
#if (defined(__linux__) && !defined(BASALT_IO_FORCE_POLL))
        std::array<epoll_event, 128> events{};
        const int count = ::epoll_wait(impl_->queue, events.data(), static_cast<int>(events.size()), timeout);
        if (count >= 0) {
            for (int i = 0; i < count; ++i) {
                const auto& event = events[static_cast<std::size_t>(i)];
                impl_->emit(event.data.u64, (event.events & EPOLLIN) != 0, (event.events & EPOLLOUT) != 0,
                            (event.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0);
            }
            break;
        }
#elif (defined(__APPLE__) && !defined(BASALT_IO_FORCE_POLL))
        std::array<struct kevent, 128> events{};
        const timespec duration{timeout / 1000, (timeout % 1000) * 1000000L};
        const int count = ::kevent(impl_->queue, nullptr, 0, events.data(), static_cast<int>(events.size()),
                                   timeout < 0 ? nullptr : &duration);
        if (count >= 0) {
            for (int i = 0; i < count; ++i) {
                const auto& event = events[static_cast<std::size_t>(i)];
                impl_->emit(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(event.udata)),
                            event.filter == EVFILT_READ, event.filter == EVFILT_WRITE,
                            (event.flags & (EV_ERROR | EV_EOF)) != 0);
            }
            break;
        }
#else
        impl_->pollEntries.clear();
        impl_->pollSerials.clear();
        const auto append = [&](const Impl::Registration& item) {
            pollfd_type entry{};
            entry.fd = static_cast<decltype(entry.fd)>(item.handle);
            entry.events = static_cast<short>((item.interest.readable ? POLLIN : 0) |
                                              (item.interest.writable ? POLLOUT : 0) | kPeerClosedEvent);
            impl_->pollEntries.push_back(entry);
            impl_->pollSerials.push_back(item.serial);
        };
        append({impl_->wakeReader, {true, false}, 0, 0});
        for (const auto& [handle, item] : impl_->watched) {
            static_cast<void>(handle);
            append(item);
        }
#if defined(_WIN32)
        const int count = ::WSAPoll(impl_->pollEntries.data(), static_cast<ULONG>(impl_->pollEntries.size()), timeout);
#else
        const int count = ::poll(impl_->pollEntries.data(), static_cast<nfds_t>(impl_->pollEntries.size()), timeout);
#endif
        if (count >= 0) {
            for (std::size_t i = 0; i < impl_->pollEntries.size(); ++i) {
                const short flags = impl_->pollEntries[i].revents;
                if (flags != 0) {
                    impl_->emit(impl_->pollSerials[i], (flags & POLLIN) != 0, (flags & POLLOUT) != 0,
                                (flags & (POLLHUP | POLLERR | POLLNVAL | kPeerClosedEvent)) != 0);
                }
            }
            break;
        }
#endif
        const int error = lastError();
        if (!interrupted(error)) {
            return std::unexpected{error};
        }
    }
    return std::span<const IoReady>{impl_->ready};
}

std::expected<void, int> EventLoop::wake() const {
    if (impl_->error != 0) {
        return std::unexpected{impl_->error};
    }
    const char byte = 1;
    while (true) {
#if defined(_WIN32)
        const auto sent = ::send(static_cast<SOCKET>(impl_->wakeWriter), &byte, 1, 0);
#else
#if defined(MSG_NOSIGNAL)
        constexpr int kFlags = MSG_NOSIGNAL;
#else
        constexpr int kFlags = 0;
#endif
        const auto sent = ::send(static_cast<int>(impl_->wakeWriter), &byte, 1, kFlags);
#endif
        if (sent == 1) {
            return {};
        }
        const int error = lastError();
        if (wouldBlock(error)) {
            return {};
        }
        if (!interrupted(error)) {
            return std::unexpected{error};
        }
    }
}

std::expected<std::vector<std::int64_t>, int> EventLoop::waitReadable(std::optional<int> timeoutMillis) {
    const auto events = wait(timeoutMillis);
    if (!events) {
        return std::unexpected{events.error()};
    }
    std::vector<std::int64_t> handles;
    for (const IoReady& event : *events) {
        if (event.readable || event.closed) {
            handles.push_back(event.handle);
        }
    }
    return handles;
}

} // namespace basalt::wire::io
