#include <basalt/wire/io/EventLoop.hpp>

#include <algorithm>
#include <cstring>

#if defined(_WIN32)
#include <winsock2.h>
using pollfd_type = WSAPOLLFD;
#define basalt_poll WSAPoll
#else
#include <cerrno>
#include <poll.h>
using pollfd_type = pollfd;
#define basalt_poll ::poll
#endif

namespace basalt::wire::io {

void EventLoop::watch(std::int64_t handle) {
    if (handle < 0 || std::ranges::find(watched_, handle) != watched_.end()) {
        return;
    }
    watched_.push_back(handle);
}

void EventLoop::unwatch(std::int64_t handle) { std::erase(watched_, handle); }

std::expected<std::vector<std::int64_t>, int> EventLoop::waitReadable(std::optional<int> timeoutMillis) {
    ready_.clear();
    if (watched_.empty()) {
        return ready_;
    }

    pollStorage_.assign(watched_.size() * sizeof(pollfd_type), 0);
    auto* entries = reinterpret_cast<pollfd_type*>(pollStorage_.data());
    for (std::size_t i = 0; i < watched_.size(); ++i) {
#if defined(_WIN32)
        entries[i].fd = static_cast<SOCKET>(watched_[i]);
#else
        entries[i].fd = static_cast<int>(watched_[i]);
#endif
        entries[i].events = POLLIN;
        entries[i].revents = 0;
    }

    while (true) {
        const int result = basalt_poll(entries, static_cast<unsigned int>(watched_.size()), timeoutMillis.value_or(-1));
        if (result < 0) {
#if defined(_WIN32)
            return std::unexpected{WSAGetLastError()};
#else
            if (errno == EINTR) {
                continue;
            }
            return std::unexpected{errno};
#endif
        }
        break;
    }

    for (std::size_t i = 0; i < watched_.size(); ++i) {
        // A hangup or an error also makes the handle readable as far as the caller is concerned: the
        // read that follows is what turns it into an end of stream.
        if ((entries[i].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) != 0) {
            ready_.push_back(watched_[i]);
        }
    }
    return ready_;
}

} // namespace basalt::wire::io
