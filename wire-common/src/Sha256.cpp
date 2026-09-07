#include <basalt/wire/common/hash/Sha256.hpp>

#include <bit>
#include <cstdint>
#include <vector>

namespace basalt::wire::common {
namespace {

constexpr std::array<std::uint32_t, 64> kConstants = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

constexpr std::array<std::uint32_t, 8> kInitial = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                                   0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

void round(std::array<std::uint32_t, 8>& state, const std::array<std::uint32_t, 64>& schedule) {
    std::array<std::uint32_t, 8> working = state;
    for (std::size_t step = 0; step < 64; step++) {
        const std::uint32_t s1 = std::rotr(working[4], 6) ^ std::rotr(working[4], 11) ^ std::rotr(working[4], 25);
        const std::uint32_t choice = (working[4] & working[5]) ^ (~working[4] & working[6]);
        const std::uint32_t first = working[7] + s1 + choice + kConstants[step] + schedule[step];
        const std::uint32_t s0 = std::rotr(working[0], 2) ^ std::rotr(working[0], 13) ^ std::rotr(working[0], 22);
        const std::uint32_t majority =
            (working[0] & working[1]) ^ (working[0] & working[2]) ^ (working[1] & working[2]);
        const std::uint32_t second = s0 + majority;
        working[7] = working[6];
        working[6] = working[5];
        working[5] = working[4];
        working[4] = working[3] + first;
        working[3] = working[2];
        working[2] = working[1];
        working[1] = working[0];
        working[0] = first + second;
    }
    for (std::size_t index = 0; index < state.size(); index++) {
        state[index] += working[index];
    }
}

} // namespace

std::array<std::uint8_t, 32> sha256(std::span<const std::uint8_t> input) {
    std::vector<std::uint8_t> padded{input.begin(), input.end()};
    const std::uint64_t bits = static_cast<std::uint64_t>(input.size()) * 8;
    padded.push_back(0x80);
    while (padded.size() % 64 != 56) {
        padded.push_back(0);
    }
    for (std::size_t shift = 8; shift > 0; shift--) {
        padded.push_back(static_cast<std::uint8_t>(bits >> ((shift - 1) * 8)));
    }

    std::array<std::uint32_t, 8> state = kInitial;
    for (std::size_t block = 0; block < padded.size(); block += 64) {
        std::array<std::uint32_t, 64> schedule{};
        for (std::size_t word = 0; word < 16; word++) {
            const std::size_t at = block + (word * 4);
            schedule[word] =
                (static_cast<std::uint32_t>(padded[at]) << 24) | (static_cast<std::uint32_t>(padded[at + 1]) << 16) |
                (static_cast<std::uint32_t>(padded[at + 2]) << 8) | static_cast<std::uint32_t>(padded[at + 3]);
        }
        for (std::size_t word = 16; word < 64; word++) {
            const std::uint32_t s0 =
                std::rotr(schedule[word - 15], 7) ^ std::rotr(schedule[word - 15], 18) ^ (schedule[word - 15] >> 3);
            const std::uint32_t s1 =
                std::rotr(schedule[word - 2], 17) ^ std::rotr(schedule[word - 2], 19) ^ (schedule[word - 2] >> 10);
            schedule[word] = schedule[word - 16] + s0 + schedule[word - 7] + s1;
        }
        round(state, schedule);
    }

    std::array<std::uint8_t, 32> digest{};
    for (std::size_t word = 0; word < state.size(); word++) {
        digest[word * 4] = static_cast<std::uint8_t>(state[word] >> 24);
        digest[(word * 4) + 1] = static_cast<std::uint8_t>(state[word] >> 16);
        digest[(word * 4) + 2] = static_cast<std::uint8_t>(state[word] >> 8);
        digest[(word * 4) + 3] = static_cast<std::uint8_t>(state[word]);
    }
    return digest;
}

std::array<std::uint8_t, 32> sha256(std::string_view input) {
    return sha256(std::span{reinterpret_cast<const std::uint8_t*>(input.data()), input.size()});
}

} // namespace basalt::wire::common
