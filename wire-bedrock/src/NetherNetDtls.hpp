#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace basalt::wire::bedrock::nethernet {

/// The one certificate every connection of a server presents; its digest goes into every answer.
class DtlsCredentials {
public:
    DtlsCredentials();
    ~DtlsCredentials();
    DtlsCredentials(const DtlsCredentials&) = delete;
    DtlsCredentials& operator=(const DtlsCredentials&) = delete;

    [[nodiscard]] bool ok() const noexcept;

    /// SHA-256 of the certificate as `AA:BB:...`, the form an SDP carries.
    [[nodiscard]] const std::string& fingerprint() const noexcept;

    struct State;

    [[nodiscard]] State& state() const noexcept { return *state_; }

private:
    std::unique_ptr<State> state_;
};

/// DTLS 1.2 as the client over datagrams someone else moves; the peer is known only by its certificate digest.
class DtlsSession {
public:
    using Sender = std::function<void(std::span<const std::uint8_t>)>;

    DtlsSession(DtlsCredentials& credentials, std::string expectedFingerprint, Sender sender);
    ~DtlsSession();
    DtlsSession(const DtlsSession&) = delete;
    DtlsSession& operator=(const DtlsSession&) = delete;

    void feed(std::span<const std::uint8_t> datagram);

    /// Drives the handshake and the retransmission timers, and collects what the peer sent.
    void pump(std::vector<std::vector<std::uint8_t>>& received);

    [[nodiscard]] bool established() const noexcept { return established_; }

    [[nodiscard]] bool failed() const noexcept { return failed_; }

    [[nodiscard]] bool send(std::span<const std::uint8_t> data);

    void close();

private:
    struct State;

    std::unique_ptr<State> state_;
    bool established_ = false;
    bool failed_ = false;
};

} // namespace basalt::wire::bedrock::nethernet
