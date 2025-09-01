#include <basalt/wire/common/Mth.hpp>

#include <array>
#include <cmath>

namespace basalt::wire::common::mth {
namespace {

std::array<float, 65536> buildSineTable() {
    std::array<float, 65536> table{};
    for (std::size_t index = 0; index < table.size(); ++index) {
        table[index] = static_cast<float>(std::sin(static_cast<double>(index) / kSinScale));
    }
    return table;
}

} // namespace

const float* sineTable() noexcept {
    static const std::array<float, 65536> table = buildSineTable();
    return table.data();
}

} // namespace basalt::wire::common::mth
