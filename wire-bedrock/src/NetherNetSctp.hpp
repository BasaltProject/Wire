#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace basalt::wire::bedrock::nethernet {

enum class SctpSend : std::uint8_t { Sent, WouldBlock, Failed };

struct SctpEvent {
    enum class Kind : std::uint8_t { Established, Closed, ChannelOpened, Message };

    Kind kind = Kind::Message;
    std::uint16_t stream = 0;
    std::string label;
    bool ordered = true;
    std::uint8_t channelKind = 0;
    std::vector<std::uint8_t> data;
};

/// One SCTP association over DTLS, as the initiating side, with the data channel handshake on top.
///
/// usrsctp calls back from threads of its own, so those callbacks only queue and `wake`; everything
/// else is meant for a single owner thread.
class SctpEngine {
public:
    /// Called from any thread when there is output or events to collect.
    using Wake = std::function<void()>;

    explicit SctpEngine(Wake wake);
    ~SctpEngine();
    SctpEngine(const SctpEngine&) = delete;
    SctpEngine& operator=(const SctpEngine&) = delete;

    [[nodiscard]] bool connect();

    void input(std::span<const std::uint8_t> packet);

    void takeOutput(std::vector<std::vector<std::uint8_t>>& packets);

    void takeEvents(std::vector<SctpEvent>& events);

    [[nodiscard]] SctpSend send(std::uint16_t stream, std::uint32_t ppid, bool ordered,
                                std::span<const std::uint8_t> data);

    /// Answers a data channel open request.
    [[nodiscard]] SctpSend acknowledge(std::uint16_t stream);

    void shutdown();

    struct Impl;

private:
    std::shared_ptr<Impl> impl_;
};

} // namespace basalt::wire::bedrock::nethernet
