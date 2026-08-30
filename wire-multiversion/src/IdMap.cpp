#include <basalt/wire/multiversion/IdMap.hpp>

#include <algorithm>
#include <charconv>

namespace basalt::wire::multiversion {
namespace {

std::int32_t toInt(std::string_view text) noexcept {
    std::int32_t value = 0;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

std::vector<std::string_view> split(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    if (text.empty()) {
        return parts;
    }
    std::size_t start = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == separator) {
            parts.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    return parts;
}

} // namespace

void IdMap::parse() const {
    const std::string_view encoded{encoded_};
    const std::size_t pipe = encoded.find('|');
    const std::string_view runs = pipe == std::string_view::npos ? encoded : encoded.substr(0, pipe);
    const std::string_view tail = pipe == std::string_view::npos ? std::string_view{} : encoded.substr(pipe + 1);

    for (const std::string_view part : split(runs, ';')) {
        const std::size_t first = part.find(':');
        const std::size_t second = part.find(':', first + 1);
        starts_.push_back(toInt(part.substr(0, first)));
        lengths_.push_back(toInt(part.substr(first + 1, second - first - 1)));
        deltas_.push_back(toInt(part.substr(second + 1)));
    }

    for (const std::string_view hole : split(tail, ';')) {
        const std::size_t arrow = hole.find('>');
        substituteFrom_.push_back(toInt(hole.substr(0, arrow)));
        substituteTo_.push_back(toInt(hole.substr(arrow + 1)));
    }
    parsed_ = true;
}

std::int32_t IdMap::map(std::int32_t id) const {
    if (!parsed_) {
        parse();
    }
    // The runs are sorted, so the right one is a binary search rather than a scan of fifty-five.
    std::size_t low = 0;
    std::size_t high = starts_.size();
    while (low < high) {
        const std::size_t mid = low + (high - low) / 2;
        if (id < starts_[mid]) {
            high = mid;
        } else if (id >= starts_[mid] + lengths_[mid]) {
            low = mid + 1;
        } else {
            return id + deltas_[mid];
        }
    }

    const auto found = std::ranges::lower_bound(substituteFrom_, id);
    if (found != substituteFrom_.end() && *found == id) {
        return substituteTo_[static_cast<std::size_t>(found - substituteFrom_.begin())];
    }
    return kNone;
}

bool IdMap::substitutes(std::int32_t id) const {
    if (!parsed_) {
        parse();
    }
    const auto found = std::ranges::lower_bound(substituteFrom_, id);
    return found != substituteFrom_.end() && *found == id;
}

std::int32_t IdMap::unmap(std::int32_t id) const {
    if (!parsed_) {
        parse();
    }
    // Backwards the runs are not sorted by their destination, so this one is a scan.
    for (std::size_t i = 0; i < starts_.size(); ++i) {
        const std::int32_t from = starts_[i] + deltas_[i];
        if (id >= from && id < from + lengths_[i]) {
            return id - deltas_[i];
        }
    }
    return kNone;
}

const IdMap& IdMap::identity() {
    static const IdMap instance{"0:2147483647:0|"};
    return instance;
}

void NameIdMap::parse() const {
    for (const std::string_view entry : split(encoded_, ';')) {
        // A name has colons of its own, so the id is what follows the last one.
        const std::size_t colon = entry.rfind(':');
        names_.emplace_back(entry.substr(0, colon));
        ids_.push_back(toInt(entry.substr(colon + 1)));
    }
    parsed_ = true;
}

bool NameIdMap::empty() const {
    if (!parsed_) {
        parse();
    }
    return names_.empty();
}

std::int32_t NameIdMap::id(std::string_view name) const {
    if (!parsed_) {
        parse();
    }
    const auto found = std::ranges::lower_bound(names_, name);
    if (found != names_.end() && *found == name) {
        return ids_[static_cast<std::size_t>(found - names_.begin())];
    }
    return kNone;
}

std::string_view NameIdMap::name(std::int32_t id) const {
    if (!parsed_) {
        parse();
    }
    for (std::size_t i = 0; i < ids_.size(); ++i) {
        if (ids_[i] == id) {
            return names_[i];
        }
    }
    return {};
}

const NameIdMap& NameIdMap::empty_() {
    static const NameIdMap instance{""};
    return instance;
}

void NameSet::parse() const {
    for (const std::string_view name : split(encoded_, ';')) {
        names_.emplace_back(name);
    }
    // Sorted so membership is a binary search: these are asked about once per registry entry.
    std::ranges::sort(names_);
    parsed_ = true;
}

bool NameSet::empty() const {
    if (!parsed_) {
        parse();
    }
    return names_.empty();
}

const std::vector<std::string>& NameSet::names() const {
    if (!parsed_) {
        parse();
    }
    return names_;
}

bool NameSet::contains(std::string_view name) const {
    if (!parsed_) {
        parse();
    }
    const auto found = std::ranges::lower_bound(names_, name);
    return found != names_.end() && *found == name;
}

} // namespace basalt::wire::multiversion
