#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct lua_State;

namespace basalt::wire::lua {
/// Where a plugin's Lua code was when the interpreter paused, kept as a call tree of samples.
class LuaProfile {
public:
    /// Records the stack of `state`, from the outermost frame in; called from the instruction-count hook.
    void sample(lua_State* state);

    /// Forgets everything and starts a new profile.
    void reset();

    [[nodiscard]] std::int64_t samples() const;

    /// The tree as JSON children of a node that took `totalMillis`, which is split by each frame's share of samples.
    [[nodiscard]] std::string childrenJson(double totalMillis) const;

    /// The per-window series of the tree's root, as members of a JSON object, scaled the same way.
    [[nodiscard]] std::string windowsJson(double totalMillis) const;

private:
    struct Node {
        std::int64_t hits = 0;
        std::vector<std::int64_t> windows;
        std::map<std::string, std::unique_ptr<Node>> children;
    };

    static void hit(Node& node, std::size_t window);
    static void write(std::string& out, const Node& node, double perSample);

    mutable std::mutex mutex_;
    Node root_;
    std::int64_t startedAtNanos_ = 0;
    bool started_ = false;
};
} // namespace basalt::wire::lua
