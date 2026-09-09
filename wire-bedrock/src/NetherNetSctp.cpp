#include "NetherNetSctp.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <usrsctp.h>
#include <utility>

namespace basalt::wire::bedrock::nethernet {
namespace {

constexpr std::uint16_t kSctpPort = 5000;
constexpr std::uint32_t kPpidDcep = 50;
constexpr std::uint8_t kDcepOpen = 0x03;
constexpr std::uint8_t kDcepAck = 0x02;
constexpr std::uint16_t kPathMtu = 1200;
constexpr int kBufferBytes = 4 * 1024 * 1024;

} // namespace

struct SctpEngine::Impl {
    std::mutex lock;
    std::deque<std::vector<std::uint8_t>> output;
    std::vector<SctpEvent> events;
    std::vector<std::uint8_t> partial;
    Wake wake;
    struct socket* handle = nullptr;
    bool dead = false;

    void notify() const {
        if (wake) {
            wake();
        }
    }
};

namespace {

std::mutex& registryLock() {
    static std::mutex lock;
    return lock;
}

std::unordered_map<const void*, std::shared_ptr<SctpEngine::Impl>>& registry() {
    static std::unordered_map<const void*, std::shared_ptr<SctpEngine::Impl>> map;
    return map;
}

bool stackReady = false;

std::shared_ptr<SctpEngine::Impl> lookup(const void* key) {
    const std::lock_guard<std::mutex> guard{registryLock()};
    const auto found = registry().find(key);
    return found == registry().end() ? nullptr : found->second;
}

int outputCallback(void* address, void* data, std::size_t length, std::uint8_t /*tos*/, std::uint8_t /*df*/) {
    const auto impl = lookup(address);
    if (impl == nullptr) {
        return -1;
    }
    {
        const std::lock_guard<std::mutex> guard{impl->lock};
        if (impl->dead) {
            return -1;
        }
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        impl->output.emplace_back(bytes, bytes + length);
    }
    impl->notify();
    return 0;
}

void handleNotification(SctpEngine::Impl& impl, const sctp_notification& note) {
    if (note.sn_header.sn_type == SCTP_ASSOC_CHANGE) {
        const std::uint16_t state = note.sn_assoc_change.sac_state;
        SctpEvent event;
        if (state == SCTP_COMM_UP) {
            event.kind = SctpEvent::Kind::Established;
        } else if (state == SCTP_COMM_LOST || state == SCTP_SHUTDOWN_COMP || state == SCTP_CANT_STR_ASSOC) {
            event.kind = SctpEvent::Kind::Closed;
        } else {
            return;
        }
        impl.events.push_back(std::move(event));
    } else if (note.sn_header.sn_type == SCTP_SHUTDOWN_EVENT) {
        SctpEvent event;
        event.kind = SctpEvent::Kind::Closed;
        impl.events.push_back(std::move(event));
    }
}

void handleDcep(SctpEngine::Impl& impl, std::uint16_t stream, const std::vector<std::uint8_t>& message) {
    if (message.empty() || message[0] != kDcepOpen || message.size() < 12) {
        return;
    }
    const std::uint8_t channelType = message[1];
    const std::size_t labelLength = (static_cast<std::size_t>(message[8]) << 8U) | message[9];
    const std::size_t protocolLength = (static_cast<std::size_t>(message[10]) << 8U) | message[11];
    if (message.size() < 12 + labelLength + protocolLength) {
        return;
    }
    SctpEvent event;
    event.kind = SctpEvent::Kind::ChannelOpened;
    event.stream = stream;
    event.label.assign(message.begin() + 12, message.begin() + 12 + static_cast<std::ptrdiff_t>(labelLength));
    event.ordered = (channelType & 0x80U) == 0;
    event.channelKind = static_cast<std::uint8_t>(channelType & 0x7fU);
    impl.events.push_back(std::move(event));
}

int receiveCallback(struct socket* /*sock*/, sctp_sockstore /*address*/, void* data, std::size_t length,
                    sctp_rcvinfo info, int flags, void* ulp) {
    const auto impl = lookup(ulp);
    if (impl == nullptr) {
        std::free(data);
        return 1;
    }
    bool wake = false;
    {
        const std::lock_guard<std::mutex> guard{impl->lock};
        if (data != nullptr) {
            if ((flags & MSG_NOTIFICATION) != 0) {
                handleNotification(*impl, *static_cast<const sctp_notification*>(data));
                wake = true;
            } else {
                const auto* bytes = static_cast<const std::uint8_t*>(data);
                impl->partial.insert(impl->partial.end(), bytes, bytes + length);
                if ((flags & MSG_EOR) != 0) {
                    std::vector<std::uint8_t> message = std::exchange(impl->partial, {});
                    const std::uint32_t ppid = ntohl(info.rcv_ppid);
                    if (ppid == kPpidDcep) {
                        handleDcep(*impl, info.rcv_sid, message);
                    } else {
                        SctpEvent event;
                        event.kind = SctpEvent::Kind::Message;
                        event.stream = info.rcv_sid;
                        event.data = std::move(message);
                        impl->events.push_back(std::move(event));
                    }
                    wake = true;
                }
            }
        }
    }
    std::free(data);
    if (wake) {
        impl->notify();
    }
    return 1;
}

int sendReadyCallback(struct socket* /*sock*/, std::uint32_t /*free*/, void* ulp) {
    if (const auto impl = lookup(ulp); impl != nullptr) {
        impl->notify();
    }
    return 0;
}

void initializeStack() {
    usrsctp_init(0, outputCallback, nullptr);
    usrsctp_sysctl_set_sctp_ecn_enable(0);
    usrsctp_sysctl_set_sctp_blackhole(2);
    usrsctp_sysctl_set_sctp_init_rto_max_default(10000);
    usrsctp_sysctl_set_sctp_rto_max_default(10000);
    usrsctp_sysctl_set_sctp_heartbeat_interval_default(10000);
    usrsctp_sysctl_set_sctp_enable_sack_immediately(1);
}

} // namespace

SctpEngine::SctpEngine(Wake wake) : impl_(std::make_shared<Impl>()) {
    impl_->wake = std::move(wake);
    const std::lock_guard<std::mutex> guard{registryLock()};
    if (!stackReady) {
        initializeStack();
        stackReady = true;
    }
    registry()[impl_.get()] = impl_;
}

SctpEngine::~SctpEngine() {
    shutdown();
    {
        const std::lock_guard<std::mutex> guard{registryLock()};
        registry().erase(impl_.get());
    }
}

bool SctpEngine::connect() {
    Impl& impl = *impl_;
    usrsctp_register_address(&impl);
    struct socket* handle =
        usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP, receiveCallback, sendReadyCallback, 0, &impl);
    if (handle == nullptr) {
        return false;
    }
    impl.handle = handle;
    usrsctp_set_non_blocking(handle, 1);

    sctp_event event{};
    event.se_assoc_id = SCTP_ALL_ASSOC;
    event.se_on = 1;
    for (const int type : {SCTP_ASSOC_CHANGE, SCTP_SHUTDOWN_EVENT}) {
        event.se_type = static_cast<std::uint16_t>(type);
        usrsctp_setsockopt(handle, IPPROTO_SCTP, SCTP_EVENT, &event, sizeof(event));
    }

    const int on = 1;
    usrsctp_setsockopt(handle, IPPROTO_SCTP, SCTP_RECVRCVINFO, &on, sizeof(on));
    usrsctp_setsockopt(handle, IPPROTO_SCTP, SCTP_NODELAY, &on, sizeof(on));
    const int buffer = kBufferBytes;
    usrsctp_setsockopt(handle, SOL_SOCKET, SO_SNDBUF, &buffer, sizeof(buffer));
    usrsctp_setsockopt(handle, SOL_SOCKET, SO_RCVBUF, &buffer, sizeof(buffer));

    sctp_assoc_value reset{};
    reset.assoc_value = SCTP_ENABLE_RESET_STREAM_REQ | SCTP_ENABLE_CHANGE_ASSOC_REQ;
    usrsctp_setsockopt(handle, IPPROTO_SCTP, SCTP_ENABLE_STREAM_RESET, &reset, sizeof(reset));

    sctp_initmsg init{};
    init.sinit_num_ostreams = 1024;
    init.sinit_max_instreams = 1024;
    usrsctp_setsockopt(handle, IPPROTO_SCTP, SCTP_INITMSG, &init, sizeof(init));

    sctp_paddrparams parameters{};
    parameters.spp_flags = SPP_PMTUD_DISABLE;
    parameters.spp_pathmtu = kPathMtu;
    usrsctp_setsockopt(handle, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS, &parameters, sizeof(parameters));

    sockaddr_conn local{};
    local.sconn_family = AF_CONN;
    local.sconn_port = htons(kSctpPort);
    local.sconn_addr = &impl;
    if (usrsctp_bind(handle, reinterpret_cast<sockaddr*>(&local), sizeof(local)) != 0) {
        return false;
    }
    const int result = usrsctp_connect(handle, reinterpret_cast<sockaddr*>(&local), sizeof(local));
    return result == 0 || errno == EINPROGRESS;
}

void SctpEngine::input(std::span<const std::uint8_t> packet) {
    usrsctp_conninput(impl_.get(), packet.data(), packet.size(), 0);
}

void SctpEngine::takeOutput(std::vector<std::vector<std::uint8_t>>& packets) {
    const std::lock_guard<std::mutex> guard{impl_->lock};
    while (!impl_->output.empty()) {
        packets.push_back(std::move(impl_->output.front()));
        impl_->output.pop_front();
    }
}

void SctpEngine::takeEvents(std::vector<SctpEvent>& events) {
    const std::lock_guard<std::mutex> guard{impl_->lock};
    for (SctpEvent& event : impl_->events) {
        events.push_back(std::move(event));
    }
    impl_->events.clear();
}

SctpSend SctpEngine::send(std::uint16_t stream, std::uint32_t ppid, bool ordered, std::span<const std::uint8_t> data) {
    if (impl_->handle == nullptr) {
        return SctpSend::Failed;
    }
    sctp_sendv_spa spa{};
    spa.sendv_flags = SCTP_SEND_SNDINFO_VALID;
    spa.sendv_sndinfo.snd_sid = stream;
    spa.sendv_sndinfo.snd_flags = static_cast<std::uint16_t>(SCTP_EOR | (ordered ? 0 : SCTP_UNORDERED));
    spa.sendv_sndinfo.snd_ppid = htonl(ppid);
    const auto written =
        usrsctp_sendv(impl_->handle, data.data(), data.size(), nullptr, 0, &spa, sizeof(spa), SCTP_SENDV_SPA, 0);
    if (written >= 0) {
        return SctpSend::Sent;
    }
    return errno == EWOULDBLOCK || errno == EAGAIN ? SctpSend::WouldBlock : SctpSend::Failed;
}

SctpSend SctpEngine::acknowledge(std::uint16_t stream) {
    constexpr std::array<std::uint8_t, 1> kAck = {kDcepAck};
    return send(stream, kPpidDcep, true, kAck);
}

void SctpEngine::shutdown() {
    Impl& impl = *impl_;
    {
        const std::lock_guard<std::mutex> guard{impl.lock};
        if (impl.dead) {
            return;
        }
        impl.dead = true;
    }
    if (impl.handle != nullptr) {
        usrsctp_close(impl.handle);
        impl.handle = nullptr;
        usrsctp_deregister_address(&impl);
    }
}

} // namespace basalt::wire::bedrock::nethernet
