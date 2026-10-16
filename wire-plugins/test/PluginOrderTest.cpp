#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/wire/plugins/PluginOrder.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using namespace basalt::wire::plugins;

namespace {
[[nodiscard]] PluginManifest manifest(std::string id, std::string version = "1.0.0",
                                      std::int32_t api = PluginManifest::kApi,
                                      std::vector<std::pair<std::string, std::string>> depends = {},
                                      std::vector<std::pair<std::string, std::string>> soft = {}) {
    PluginManifest built;
    built.name = id;
    built.id = std::move(id);
    built.version = std::move(version);
    built.api = api;
    built.main = "src/main.lua";
    for (auto& [dependencyId, requirement] : depends) {
        built.dependencies.push_back(
            PluginDependency{.id = std::move(dependencyId), .requirement = std::move(requirement), .hard = true});
    }
    for (auto& [dependencyId, requirement] : soft) {
        built.dependencies.push_back(
            PluginDependency{.id = std::move(dependencyId), .requirement = std::move(requirement), .hard = false});
    }
    return built;
}

[[nodiscard]] std::string describe(const std::vector<PluginOrdering>& orderings) {
    std::string text;
    for (const PluginOrdering& ordering : orderings) {
        if (!text.empty()) {
            text += ';';
        }
        text +=
            ordering.loads() ? "+" + ordering.manifest.id : "-" + ordering.manifest.id + "(" + ordering.refusal + ")";
    }
    return text;
}
} // namespace

TEST_CASE("what loads and in what order is what the frozen kotlin decides") {
    CHECK(describe(pluginOrder::resolve({manifest("zebra"), manifest("apple"), manifest("mango")})) ==
          "+apple;+mango;+zebra");

    CHECK(describe(pluginOrder::resolve({manifest("app", "1.0.0", 1, {{"core", ">=1.0"}}),
                                         manifest("core", "1.0.0", 1, {{"base", "*"}}), manifest("base")})) ==
          "+base;+core;+app");

    CHECK(describe(pluginOrder::resolve({manifest("addon", "1.0.0", 1, {}, {{"zbase", "*"}}), manifest("zbase")})) ==
          "+zbase;+addon");
    CHECK(describe(pluginOrder::resolve({manifest("addon", "1.0.0", 1, {}, {{"gone", "*"}})})) == "+addon");

    CHECK(describe(pluginOrder::resolve({})).empty());
}

TEST_CASE("a refusal spreads down the chain and a cycle is named as a group") {
    CHECK(describe(pluginOrder::resolve({manifest("far", "1.0.0", 1, {{"leaf", "*"}}),
                                         manifest("leaf", "1.0.0", 1, {{"mid", "*"}}),
                                         manifest("mid", "1.0.0", 1, {{"root", "*"}})})) ==
          "-far('leaf' did not load);-leaf('mid' did not load);-mid(it needs 'root', which is not installed)");

    CHECK(describe(pluginOrder::resolve({manifest("loop", "1.0.0", 1, {{"loop", "*"}})})) ==
          "-loop(it is in a dependency cycle with: loop)");

    CHECK(describe(pluginOrder::resolve({manifest("alpha", "1.0.0", 1, {{"beta", "*"}}),
                                         manifest("beta", "1.0.0", 1, {{"gamma", "*"}}),
                                         manifest("gamma", "1.0.0", 1, {{"alpha", "*"}}), manifest("free")})) ==
          "+free;-alpha(it is in a dependency cycle with: alpha, beta, gamma);"
          "-beta(it is in a dependency cycle with: alpha, beta, gamma);"
          "-gamma(it is in a dependency cycle with: alpha, beta, gamma)");
}
