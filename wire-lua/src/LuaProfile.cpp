#include <basalt/wire/lua/LuaProfile.hpp>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace basalt::wire::lua {
namespace {
constexpr std::int64_t kWindowNanos = 10'000'000'000;
constexpr std::size_t kMaxWindows = 180;
constexpr int kMaxDepth = 64;
constexpr std::size_t kMaxNodes = 20'000;

[[nodiscard]] std::string frameName(lua_Debug& info) {
    std::string source = info.short_src;
    if (!source.empty() && source.front() == '@') {
        source.erase(0, 1);
    }
    if (std::string_view{info.what} == "C") {
        return std::string{"[host] "} + (info.name != nullptr ? info.name : "?");
    }
    if (std::string_view{info.what} == "main") {
        return source + " (main chunk)";
    }
    return std::string{info.name != nullptr ? info.name : "function"} + " (" + source + ":" +
           std::to_string(info.linedefined) + ")";
}

void quote(std::string& out, const std::string& text) {
    out += '"';
    for (const char c : text) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (static_cast<unsigned char>(c) < 0x20) {
            out += ' ';
        } else {
            out += c;
        }
    }
    out += '"';
}

[[nodiscard]] std::string millis(double value) {
    char buffer[40];
    std::snprintf(buffer, sizeof(buffer), "%.3f", value);
    return buffer;
}
} // namespace

void LuaProfile::hit(Node& node, std::size_t window) {
    node.hits++;
    if (node.windows.size() <= window) {
        node.windows.resize(window + 1, 0);
    }
    node.windows[window]++;
}

void LuaProfile::sample(lua_State* state) {
    std::vector<std::string> frames;
    lua_Debug info;
    for (int level = 0; level < kMaxDepth && lua_getstack(state, level, &info) != 0; ++level) {
        if (lua_getinfo(state, "nSl", &info) != 0) {
            frames.push_back(frameName(info));
        }
    }
    const std::int64_t now =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();
    const std::lock_guard<std::mutex> lock{mutex_};
    if (!started_) {
        started_ = true;
        startedAtNanos_ = now;
    }
    const std::size_t window =
        std::min<std::size_t>(static_cast<std::size_t>((now - startedAtNanos_) / kWindowNanos), kMaxWindows - 1);
    hit(root_, window);
    Node* node = &root_;
    std::size_t known = 0;
    for (auto name = frames.rbegin(); name != frames.rend(); ++name) {
        auto found = node->children.find(*name);
        if (found == node->children.end()) {
            if (known >= kMaxNodes) {
                return;
            }
            found = node->children.emplace(*name, std::make_unique<Node>()).first;
            ++known;
        }
        node = found->second.get();
        hit(*node, window);
    }
}

void LuaProfile::reset() {
    const std::lock_guard<std::mutex> lock{mutex_};
    root_ = Node{};
    started_ = false;
}

std::int64_t LuaProfile::samples() const {
    const std::lock_guard<std::mutex> lock{mutex_};
    return root_.hits;
}

void LuaProfile::write(std::string& out, const Node& node, double perSample) {
    bool comma = false;
    for (const auto& [name, child] : node.children) {
        if (comma) {
            out += ',';
        }
        comma = true;
        std::int64_t below = 0;
        for (const auto& grandchild : child->children) {
            below += grandchild.second->hits;
        }
        out += "{\"name\":";
        quote(out, name);
        out += ",\"calls\":" + std::to_string(child->hits) + ",\"total_millis\":" + millis(static_cast<double>(child->hits) * perSample) +
               ",\"self_millis\":" + millis(static_cast<double>(std::max<std::int64_t>(child->hits - below, 0)) * perSample) +
               ",\"max_millis\":0,\"window_calls\":[";
        for (std::size_t i = 0; i < child->windows.size(); ++i) {
            out += (i > 0 ? "," : "") + std::to_string(child->windows[i]);
        }
        out += "],\"window_millis\":[";
        for (std::size_t i = 0; i < child->windows.size(); ++i) {
            out += (i > 0 ? "," : "") + millis(static_cast<double>(child->windows[i]) * perSample);
        }
        out += "],\"children\":[";
        write(out, *child, perSample);
        out += "]}";
    }
}

std::string LuaProfile::windowsJson(double totalMillis) const {
    const std::lock_guard<std::mutex> lock{mutex_};
    const double perSample = root_.hits > 0 ? totalMillis / static_cast<double>(root_.hits) : 0.0;
    std::string out = "\"window_calls\":[";
    for (std::size_t i = 0; i < root_.windows.size(); ++i) {
        out += (i > 0 ? "," : "") + std::to_string(root_.windows[i]);
    }
    out += "],\"window_millis\":[";
    for (std::size_t i = 0; i < root_.windows.size(); ++i) {
        out += (i > 0 ? "," : "") + millis(static_cast<double>(root_.windows[i]) * perSample);
    }
    return out + "]";
}

std::string LuaProfile::childrenJson(double totalMillis) const {
    const std::lock_guard<std::mutex> lock{mutex_};
    std::string out;
    if (root_.hits > 0) {
        write(out, root_, totalMillis / static_cast<double>(root_.hits));
    }
    return out;
}
} // namespace basalt::wire::lua
