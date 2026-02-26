#include <basalt/wire/bedrock/RakSession.hpp>

#include <algorithm>
#include <iterator>
#include <utility>

namespace basalt::wire::bedrock {
namespace {

/// Enough parts for a message of a few megabytes at the smallest MTU, and a ceiling so a peer
/// cannot ask for an allocation by claiming a split of four billion.
constexpr std::uint32_t kMaxSplitParts = 8192;

/// How far ahead of what we have seen a datagram may claim to be before we stop counting the gap
/// and just follow it. A peer that jumps further than this has restarted its numbering.
constexpr std::uint32_t kMaxGap = 4096;

constexpr std::size_t kReliableWindow = 2048;

/// 127.0.0.1, in the network order the endpoint keeps.
constexpr std::uint32_t kLoopback = 0x0100007fU;

} // namespace

RakSession::RakSession(io::UdpEndpoint peer, std::uint16_t mtu, std::uint64_t clientGuid, std::uint64_t nowMs)
    : peer_(peer), mtu_(std::clamp(mtu, kMinimumMtu, kMaximumMtu)), clientGuid_(clientGuid), lastHeardMs_(nowMs),
      lastSentMs_(nowMs) {}

std::size_t RakSession::maxPayload() const noexcept {
    return static_cast<std::size_t>(mtu_) - kIpv4Overhead - kDatagramHeaderSize;
}

void RakSession::receiveDatagram(std::span<const std::uint8_t> datagram, std::uint64_t nowMs,
                                 std::vector<std::vector<std::uint8_t>>& messages) {
    if (datagram.empty()) {
        return;
    }
    lastHeardMs_ = nowMs;

    inbound_.clear();
    inbound_.writeBytes(datagram);

    const std::uint8_t flags = inbound_.readByte();
    if ((flags & kFlagValid) == 0) {
        return;
    }

    if ((flags & kFlagAck) != 0) {
        std::vector<SequenceRange> ranges;
        if (readRanges(inbound_, ranges)) {
            removeAcknowledged(ranges);
        }
        return;
    }
    if ((flags & kFlagNak) != 0) {
        std::vector<SequenceRange> ranges;
        if (readRanges(inbound_, ranges)) {
            requeueNaked(ranges);
        }
        return;
    }

    const std::uint32_t sequence = readMediumLE(inbound_);
    if (inbound_.failed()) {
        return;
    }
    ackQueue_.push_back(sequence);
    if (sequence >= expectedSequence_) {
        if (sequence - expectedSequence_ <= kMaxGap) {
            for (std::uint32_t missing = expectedSequence_; missing < sequence; ++missing) {
                nakQueue_.push_back(missing);
            }
        }
        expectedSequence_ = sequence + 1;
    }

    while (inbound_.readableBytes() > 0) {
        RakFrame frame;
        if (!RakFrame::decode(inbound_, frame)) {
            return;
        }
        handleFrame(frame, nowMs, messages);
    }
}

void RakSession::handleFrame(RakFrame& frame, std::uint64_t nowMs, std::vector<std::vector<std::uint8_t>>& messages) {
    if (isReliable(frame.reliability)) {
        if (!seenReliable_.insert(frame.reliableIndex).second) {
            return;
        }
        highestReliable_ = std::max(highestReliable_, frame.reliableIndex);
        if (seenReliable_.size() > kReliableWindow * 2) {
            const std::uint32_t floor = highestReliable_ > kReliableWindow ? highestReliable_ - kReliableWindow : 0;
            std::erase_if(seenReliable_, [floor](std::uint32_t index) { return index < floor; });
        }
    }

    if (frame.split) {
        if (frame.partCount == 0 || frame.partCount > kMaxSplitParts || frame.partIndex >= frame.partCount) {
            return;
        }
        SplitBuffer& buffer = splits_[frame.partId];
        if (buffer.partCount != frame.partCount) {
            buffer = SplitBuffer{};
            buffer.partCount = frame.partCount;
            buffer.startedMs = nowMs;
            buffer.parts.resize(frame.partCount);
        }
        if (!buffer.parts[frame.partIndex].empty()) {
            return;
        }
        buffer.parts[frame.partIndex] = std::move(frame.body);
        ++buffer.received;
        if (buffer.received < buffer.partCount) {
            return;
        }

        std::size_t total = 0;
        for (const std::vector<std::uint8_t>& part : buffer.parts) {
            total += part.size();
        }
        std::vector<std::uint8_t> whole;
        whole.reserve(total);
        for (std::vector<std::uint8_t>& part : buffer.parts) {
            whole.insert(whole.end(), part.begin(), part.end());
        }
        splits_.erase(frame.partId);
        frame.body = std::move(whole);
        frame.split = false;
    }

    OrderChannel& channel = channels_[frame.orderChannel % kOrderChannels];

    if (isSequenced(frame.reliability)) {
        if (channel.anySequence && frame.sequenceIndex < channel.highestSequence) {
            return;
        }
        channel.highestSequence = frame.sequenceIndex;
        channel.anySequence = true;
        deliver(std::move(frame.body), messages);
        return;
    }

    if (isOrdered(frame.reliability)) {
        if (frame.orderIndex < channel.expectedIndex) {
            return;
        }
        if (frame.orderIndex > channel.expectedIndex) {
            channel.held.emplace(frame.orderIndex, std::move(frame.body));
            return;
        }
        ++channel.expectedIndex;
        deliver(std::move(frame.body), messages);
        for (auto held = channel.held.find(channel.expectedIndex); held != channel.held.end();
             held = channel.held.find(channel.expectedIndex)) {
            std::vector<std::uint8_t> body = std::move(held->second);
            channel.held.erase(held);
            ++channel.expectedIndex;
            deliver(std::move(body), messages);
        }
        return;
    }

    deliver(std::move(frame.body), messages);
}

void RakSession::deliver(std::vector<std::uint8_t>&& message, std::vector<std::vector<std::uint8_t>>& messages) {
    handleMessage(std::move(message), lastHeardMs_, messages);
}

void RakSession::handleMessage(std::vector<std::uint8_t>&& message, std::uint64_t nowMs,
                               std::vector<std::vector<std::uint8_t>>& messages) {
    if (message.empty()) {
        return;
    }

    switch (static_cast<RakId>(message[0])) {
    case RakId::Game:
        messages.emplace_back(message.begin() + 1, message.end());
        return;
    case RakId::ConnectionRequest:
        reader_.clear();
        reader_.writeBytes(std::span<const std::uint8_t>{message}.subspan(1));
        onConnectionRequest(reader_, nowMs);
        return;
    case RakId::ConnectedPing:
        reader_.clear();
        reader_.writeBytes(std::span<const std::uint8_t>{message}.subspan(1));
        onConnectedPing(reader_, nowMs);
        return;
    case RakId::NewIncomingConnection:
        connected_ = true;
        return;
    case RakId::DisconnectionNotification:
        closed_ = true;
        return;
    default:
        return;
    }
}

void RakSession::onConnectionRequest(io::ByteBuffer& body, std::uint64_t nowMs) {
    const auto guid = static_cast<std::uint64_t>(body.readLong());
    const std::int64_t requestTime = body.readLong();
    // The client repeats the guid it opened the connection with. A different one on the same
    // address is a different client, and answering it would hand it someone else's session.
    if (body.failed() || guid != clientGuid_) {
        return;
    }

    scratch_.clear();
    scratch_.writeByte(static_cast<std::uint8_t>(RakId::ConnectionRequestAccepted));
    writeAddress(scratch_, peer_);
    scratch_.writeShort(0);
    // RakNet expects the ten addresses the server answers on. Only the first says anything, and a
    // Bedrock client reads none of them.
    writeAddress(scratch_, io::UdpEndpoint{kLoopback, 0});
    for (int index = 1; index < 10; ++index) {
        writeAddress(scratch_, io::UdpEndpoint{});
    }
    scratch_.writeLong(requestTime);
    scratch_.writeLong(static_cast<std::int64_t>(nowMs));
    sendMessage(scratch_.readable(), Reliability::ReliableOrdered);
}

void RakSession::onConnectedPing(io::ByteBuffer& body, std::uint64_t nowMs) {
    const std::int64_t pingTime = body.readLong();
    if (body.failed()) {
        return;
    }
    scratch_.clear();
    scratch_.writeByte(static_cast<std::uint8_t>(RakId::ConnectedPong));
    scratch_.writeLong(pingTime);
    scratch_.writeLong(static_cast<std::int64_t>(nowMs));
    sendMessage(scratch_.readable(), Reliability::Unreliable);
}

void RakSession::sendGame(std::span<const std::uint8_t> payload) {
    std::vector<std::uint8_t> message;
    message.reserve(payload.size() + 1);
    message.push_back(static_cast<std::uint8_t>(RakId::Game));
    message.insert(message.end(), payload.begin(), payload.end());
    queueFrames(message, Reliability::ReliableOrdered, 0);
}

void RakSession::sendMessage(std::span<const std::uint8_t> message, Reliability reliability, std::uint8_t channel) {
    queueFrames(message, reliability, channel);
}

void RakSession::queueFrames(std::span<const std::uint8_t> message, Reliability reliability, std::uint8_t channel) {
    if (message.empty()) {
        return;
    }
    const std::uint8_t orderChannel = channel % kOrderChannels;

    RakFrame prototype;
    prototype.reliability = reliability;
    prototype.orderChannel = orderChannel;
    if (isOrdered(reliability)) {
        prototype.orderIndex = nextOrderIndex_[orderChannel]++;
    } else if (isSequenced(reliability)) {
        prototype.orderIndex = nextOrderIndex_[orderChannel];
        prototype.sequenceIndex = nextSequenceIndex_[orderChannel]++;
    }

    prototype.body.assign(message.begin(), message.end());
    if (prototype.encodedSize() <= maxPayload()) {
        if (isReliable(reliability)) {
            prototype.reliableIndex = nextReliableIndex_++;
        }
        outgoing_.push_back(std::move(prototype));
        return;
    }

    // A split part pays ten more bytes of header, so the body that fits shrinks by exactly that.
    const std::size_t partSize = maxPayload() - (prototype.encodedSize() - message.size()) - 10;
    const std::uint32_t partCount = static_cast<std::uint32_t>((message.size() + partSize - 1) / partSize);
    const std::uint16_t partId = nextSplitId_++;

    for (std::uint32_t part = 0; part < partCount; ++part) {
        const std::size_t offset = static_cast<std::size_t>(part) * partSize;
        const std::size_t length = std::min(partSize, message.size() - offset);

        RakFrame frame = prototype;
        frame.split = true;
        frame.partCount = partCount;
        frame.partId = partId;
        frame.partIndex = part;
        frame.body.assign(message.begin() + static_cast<std::ptrdiff_t>(offset),
                          message.begin() + static_cast<std::ptrdiff_t>(offset + length));
        if (isReliable(reliability)) {
            frame.reliableIndex = nextReliableIndex_++;
        }
        outgoing_.push_back(std::move(frame));
    }
}

void RakSession::sendRanges(std::uint8_t flags, std::vector<std::uint32_t>& sequences, const DatagramSink& sink) {
    std::sort(sequences.begin(), sequences.end());
    sequences.erase(std::unique(sequences.begin(), sequences.end()), sequences.end());

    std::vector<SequenceRange> ranges;
    collapseRanges(sequences, ranges);
    sequences.clear();

    // A range costs seven bytes at worst, and the header three.
    const std::size_t perDatagram = std::max<std::size_t>(1, (maxPayload() - 3) / 7);
    for (std::size_t start = 0; start < ranges.size(); start += perDatagram) {
        const std::size_t count = std::min(perDatagram, ranges.size() - start);
        scratch_.clear();
        scratch_.writeByte(flags);
        writeRanges(scratch_, std::span<const SequenceRange>{ranges}.subspan(start, count));
        sink(scratch_.readable());
    }
}

void RakSession::removeAcknowledged(std::span<const SequenceRange> ranges) {
    for (const SequenceRange& range : ranges) {
        auto entry = unacknowledged_.lower_bound(range.start);
        while (entry != unacknowledged_.end() && entry->first <= range.end) {
            entry = unacknowledged_.erase(entry);
        }
    }
}

void RakSession::requeueNaked(std::span<const SequenceRange> ranges) {
    for (const SequenceRange& range : ranges) {
        auto entry = unacknowledged_.lower_bound(range.start);
        while (entry != unacknowledged_.end() && entry->first <= range.end) {
            outgoing_.insert(outgoing_.end(), std::make_move_iterator(entry->second.frames.begin()),
                             std::make_move_iterator(entry->second.frames.end()));
            entry = unacknowledged_.erase(entry);
        }
    }
}

void RakSession::flush(std::uint64_t nowMs, const DatagramSink& sink) {
    if (!ackQueue_.empty()) {
        sendRanges(kFlagValid | kFlagAck, ackQueue_, sink);
    }
    if (!nakQueue_.empty()) {
        sendRanges(kFlagValid | kFlagNak, nakQueue_, sink);
    }

    for (auto entry = unacknowledged_.begin(); entry != unacknowledged_.end();) {
        if (nowMs - entry->second.sentAtMs < kResendTimeoutMs) {
            ++entry;
            continue;
        }
        outgoing_.insert(outgoing_.end(), std::make_move_iterator(entry->second.frames.begin()),
                         std::make_move_iterator(entry->second.frames.end()));
        entry = unacknowledged_.erase(entry);
    }

    if (outgoing_.empty() && connected_ && nowMs - lastSentMs_ >= kKeepAliveMs) {
        scratch_.clear();
        scratch_.writeByte(static_cast<std::uint8_t>(RakId::ConnectedPing));
        scratch_.writeLong(static_cast<std::int64_t>(nowMs));
        queueFrames(scratch_.readable(), Reliability::Unreliable, 0);
    }

    std::size_t index = 0;
    while (index < outgoing_.size()) {
        const std::uint32_t sequence = nextDatagramSequence_++;
        scratch_.clear();
        scratch_.writeByte(kFlagValid);
        writeMediumLE(scratch_, sequence);

        SentDatagram record;
        std::size_t used = kDatagramHeaderSize;
        while (index < outgoing_.size()) {
            const std::size_t size = outgoing_[index].encodedSize();
            if (used + size > maxPayload() && used > kDatagramHeaderSize) {
                break;
            }
            outgoing_[index].encode(scratch_);
            used += size;
            if (isReliable(outgoing_[index].reliability)) {
                record.frames.push_back(std::move(outgoing_[index]));
            }
            ++index;
        }

        sink(scratch_.readable());
        lastSentMs_ = nowMs;
        if (!record.frames.empty()) {
            record.sentAtMs = nowMs;
            unacknowledged_.emplace(sequence, std::move(record));
        }
    }
    outgoing_.clear();
}

void RakSession::disconnect(std::uint64_t nowMs, const DatagramSink& sink) {
    if (closed_) {
        return;
    }
    scratch_.clear();
    scratch_.writeByte(static_cast<std::uint8_t>(RakId::DisconnectionNotification));
    queueFrames(scratch_.readable(), Reliability::ReliableOrdered, 0);
    flush(nowMs, sink);
    closed_ = true;
}

} // namespace basalt::wire::bedrock
