#include <basalt/wire/io/Compression.hpp>

#define ZSTD_STATIC_LINKING_ONLY
#include <algorithm>
#include <array>
#include <utility>
#include <zstd.h>
#include <zstd_errors.h>

namespace basalt::wire::io::zstd {
namespace {

constexpr std::size_t kChunkSize = 64 * 1024;

CompressionError translate(std::size_t code) noexcept {
    if (ZSTD_getErrorCode(code) == ZSTD_error_memory_allocation) {
        return CompressionError::OutOfMemory;
    }
    return CompressionError::CorruptData;
}

} // namespace

std::expected<std::vector<std::uint8_t>, CompressionError> compress(std::span<const std::uint8_t> source, int level) {
    if (source.empty()) {
        return std::vector<std::uint8_t>{};
    }

    std::vector<std::uint8_t> out(ZSTD_compressBound(source.size()));
    const std::size_t written = ZSTD_compress(out.data(), out.size(), source.data(), source.size(), level);
    if (ZSTD_isError(written) != 0) {
        return std::unexpected{translate(written)};
    }
    out.resize(written);
    return out;
}

std::expected<std::vector<std::uint8_t>, CompressionError> decompress(std::span<const std::uint8_t> source) {
    if (source.empty()) {
        return std::vector<std::uint8_t>{};
    }

    // The frame usually declares its uncompressed size, which makes the common case a single
    // allocation of exactly the right size.
    const unsigned long long declared = ZSTD_getFrameContentSize(source.data(), source.size());
    if (declared != ZSTD_CONTENTSIZE_UNKNOWN && declared != ZSTD_CONTENTSIZE_ERROR) {
        if (declared > kMaxDecompressedSize) {
            return std::unexpected{CompressionError::TooLarge};
        }
        std::vector<std::uint8_t> out(static_cast<std::size_t>(declared));
        const std::size_t written = ZSTD_decompress(out.data(), out.size(), source.data(), source.size());
        if (ZSTD_isError(written) != 0) {
            return std::unexpected{translate(written)};
        }
        out.resize(written);
        return out;
    }

    ZSTD_DStream* stream = ZSTD_createDStream();
    if (stream == nullptr) {
        return std::unexpected{CompressionError::OutOfMemory};
    }
    ZSTD_initDStream(stream);

    std::vector<std::uint8_t> out;
    std::array<std::uint8_t, kChunkSize> chunk{};
    ZSTD_inBuffer input{source.data(), source.size(), 0};
    while (input.pos < input.size) {
        ZSTD_outBuffer output{chunk.data(), chunk.size(), 0};
        const std::size_t result = ZSTD_decompressStream(stream, &output, &input);
        if (ZSTD_isError(result) != 0) {
            ZSTD_freeDStream(stream);
            return std::unexpected{translate(result)};
        }
        if (out.size() + output.pos > kMaxDecompressedSize) {
            ZSTD_freeDStream(stream);
            return std::unexpected{CompressionError::TooLarge};
        }
        out.insert(out.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(output.pos));
    }
    ZSTD_freeDStream(stream);
    return out;
}

struct StreamCompressor::State {
    ZSTD_CStream* stream = nullptr;
    Sink sink;
    std::array<std::uint8_t, kChunkSize> output{};
    bool failed = false;

    explicit State(Sink sink) : sink(std::move(sink)) {}

    ~State() {
        if (stream != nullptr) {
            ZSTD_freeCStream(stream);
        }
    }

    State(const State&) = delete;
    State& operator=(const State&) = delete;

    /// Drives one call of the compressor until it has nothing more to give for this input.
    bool drive(ZSTD_inBuffer& input, ZSTD_EndDirective mode) {
        while (true) {
            ZSTD_outBuffer out{output.data(), output.size(), 0};
            const std::size_t remaining = ZSTD_compressStream2(stream, &out, &input, mode);
            if (ZSTD_isError(remaining) != 0) {
                failed = true;
                return false;
            }
            if (out.pos > 0) {
                sink(std::span{output.data(), out.pos});
            }
            const bool done = mode == ZSTD_e_end ? remaining == 0 : input.pos == input.size;
            if (done) {
                return true;
            }
        }
    }
};

StreamCompressor::StreamCompressor(Sink sink, int level) : state_(std::make_unique<State>(std::move(sink))) {
    state_->stream = ZSTD_createCStream();
    if (state_->stream == nullptr) {
        state_->failed = true;
        return;
    }
    if (ZSTD_isError(ZSTD_CCtx_setParameter(state_->stream, ZSTD_c_compressionLevel, level)) != 0) {
        state_->failed = true;
    }
}

StreamCompressor::~StreamCompressor() = default;

bool StreamCompressor::valid() const noexcept { return !state_->failed; }

bool StreamCompressor::feed(std::span<const std::uint8_t> piece) {
    if (state_->failed) {
        return false;
    }
    if (piece.empty()) {
        return true;
    }
    ZSTD_inBuffer input{piece.data(), piece.size(), 0};
    return state_->drive(input, ZSTD_e_continue);
}

bool StreamCompressor::finish() {
    if (state_->failed) {
        return false;
    }
    ZSTD_inBuffer input{nullptr, 0, 0};
    return state_->drive(input, ZSTD_e_end);
}

struct StreamDecompressor::State {
    ZSTD_DStream* stream = nullptr;
    ZSTD_inBuffer input{nullptr, 0, 0};
    std::array<std::uint8_t, kChunkSize> buffer{};

    /// What of `buffer` has been decompressed but not yet handed out.
    std::size_t pendingStart = 0;
    std::size_t pendingEnd = 0;
    bool failed = false;
    bool ended = false;

    ~State() {
        if (stream != nullptr) {
            ZSTD_freeDStream(stream);
        }
    }

    State() = default;
    State(const State&) = delete;
    State& operator=(const State&) = delete;

    /// Decompresses the next block into the buffer. False once the frame has ended or failed.
    bool refill() {
        if (failed || ended || input.pos == input.size) {
            return false;
        }
        ZSTD_outBuffer out{buffer.data(), buffer.size(), 0};
        const std::size_t remaining = ZSTD_decompressStream(stream, &out, &input);
        if (ZSTD_isError(remaining) != 0) {
            failed = true;
            return false;
        }
        pendingStart = 0;
        pendingEnd = out.pos;
        if (remaining == 0) {
            ended = true;
        }
        return out.pos > 0;
    }
};

StreamDecompressor::StreamDecompressor(std::span<const std::uint8_t> source) : state_(std::make_unique<State>()) {
    state_->stream = ZSTD_createDStream();
    if (state_->stream == nullptr) {
        state_->failed = true;
        return;
    }
    ZSTD_initDStream(state_->stream);
    state_->input = ZSTD_inBuffer{source.data(), source.size(), 0};
}

StreamDecompressor::~StreamDecompressor() = default;

bool StreamDecompressor::valid() const noexcept { return !state_->failed; }

bool StreamDecompressor::readFully(std::span<std::uint8_t> destination) {
    std::size_t filled = 0;
    while (filled < destination.size()) {
        if (state_->pendingStart == state_->pendingEnd && !state_->refill()) {
            return false;
        }
        const std::size_t available = state_->pendingEnd - state_->pendingStart;
        const std::size_t take = std::min(available, destination.size() - filled);
        std::copy_n(state_->buffer.begin() + static_cast<std::ptrdiff_t>(state_->pendingStart), take,
                    destination.begin() + static_cast<std::ptrdiff_t>(filled));
        state_->pendingStart += take;
        filled += take;
    }
    return true;
}

bool StreamDecompressor::frameComplete() const noexcept {
    if (state_->failed) {
        return false;
    }
    if (state_->pendingStart != state_->pendingEnd) {
        return false;
    }
    // The frame may end on a block that filled the output buffer exactly, in which case the
    // decompressor has not been asked the one more time that tells it so: with the input consumed
    // and nothing pending there is nothing left either way.
    return state_->ended || state_->input.pos == state_->input.size;
}

} // namespace basalt::wire::io::zstd
