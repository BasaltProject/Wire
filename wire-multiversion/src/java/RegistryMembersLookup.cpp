#include <basalt/wire/multiversion/java/RegistryMembers.hpp>

#include <map>
#include <mutex>
#include <set>
#include <string>

namespace basalt::wire::multiversion::java {
namespace {

/// One version's pairs, split once and kept. A login reads a dozen registries and every element of
/// each, so splitting the list per element would be the whole table per connection.
const std::set<std::string, std::less<>>& membersOf(const std::int32_t protocol) {
    static std::mutex lock;
    static std::map<std::int32_t, std::set<std::string, std::less<>>> cache;

    const std::lock_guard<std::mutex> guard{lock};
    const auto found = cache.find(protocol);
    if (found != cache.end()) {
        return found->second;
    }

    std::set<std::string, std::less<>> split;
    const std::string_view list = registryMembersOf(protocol);
    std::size_t at = 0;
    while (at < list.size()) {
        const std::size_t end = list.find(' ', at);
        const std::size_t stop = end == std::string_view::npos ? list.size() : end;
        if (stop > at) {
            split.emplace(list.substr(at, stop - at));
        }
        at = stop + 1;
    }
    return cache.emplace(protocol, std::move(split)).first->second;
}

} // namespace

bool versionHasMember(const std::int32_t protocol, const std::string_view registry, const std::string_view member) {
    const std::set<std::string, std::less<>>& members = membersOf(protocol);
    if (members.empty()) {
        return true;
    }
    const std::string prefix = std::string{registry} + "=";
    const auto first = members.lower_bound(prefix);
    if (first == members.end() || !first->starts_with(prefix)) {
        // Nothing is kept about that registry there, so nothing can be said about its members.
        return true;
    }
    return members.contains(prefix + std::string{member});
}

} // namespace basalt::wire::multiversion::java
