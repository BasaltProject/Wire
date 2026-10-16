#include <basalt/wire/plugins/PluginOrder.hpp>

#include <algorithm>
#include <map>
#include <set>

namespace basalt::wire::plugins {
namespace {
using Refusals = std::map<std::string, std::string, std::less<>>;

void refuse(Refusals& refusals, const std::string& id, std::string reason) { refusals[id] = std::move(reason); }
} // namespace

std::vector<PluginOrdering> pluginOrder::resolve(const std::vector<PluginManifest>& candidates) {
    std::map<std::string, const PluginManifest*, std::less<>> byId;
    for (const PluginManifest& manifest : candidates) {
        byId.emplace(manifest.id, &manifest);
    }
    Refusals refusals;

    for (const PluginManifest& manifest : candidates) {
        if (manifest.api != PluginManifest::kApi) {
            refuse(refusals, manifest.id,
                   "it was written for API " + std::to_string(manifest.api) + " and this server speaks " +
                       std::to_string(PluginManifest::kApi));
        }
    }
    for (const PluginManifest& manifest : candidates) {
        if (refusals.contains(manifest.id)) {
            continue;
        }
        for (const PluginDependency& dependency : manifest.hardDependencies()) {
            const auto other = byId.find(dependency.id);
            if (other == byId.end()) {
                refuse(refusals, manifest.id, "it needs '" + dependency.id + "', which is not installed");
            } else if (!dependency.accepts(other->second->version)) {
                refuse(refusals, manifest.id,
                       "it needs '" + dependency.id + "' " + dependency.requirement + " and " + other->second->version +
                           " is installed");
            }
        }
    }

    bool spread = true;
    while (spread) {
        spread = false;
        for (const PluginManifest& manifest : candidates) {
            if (refusals.contains(manifest.id)) {
                continue;
            }
            for (const PluginDependency& dependency : manifest.hardDependencies()) {
                if (refusals.contains(dependency.id)) {
                    refuse(refusals, manifest.id, "'" + dependency.id + "' did not load");
                    spread = true;
                    break;
                }
            }
        }
    }

    std::vector<const PluginManifest*> ready;
    for (const PluginManifest& manifest : candidates) {
        if (!refusals.contains(manifest.id)) {
            ready.push_back(&manifest);
        }
    }
    std::sort(ready.begin(), ready.end(),
              [](const PluginManifest* left, const PluginManifest* right) { return left->id < right->id; });
    std::set<std::string, std::less<>> readyIds;
    for (const PluginManifest* manifest : ready) {
        readyIds.insert(manifest->id);
    }

    std::vector<const PluginManifest*> ordered;
    std::set<std::string, std::less<>> placed;
    bool progress = true;
    while (progress) {
        progress = false;
        for (const PluginManifest* manifest : ready) {
            if (placed.contains(manifest->id)) {
                continue;
            }
            const bool waiting = std::any_of(
                manifest->dependencies.begin(), manifest->dependencies.end(), [&](const PluginDependency& dependency) {
                    return readyIds.contains(dependency.id) && !placed.contains(dependency.id);
                });
            if (waiting) {
                continue;
            }
            ordered.push_back(manifest);
            placed.insert(manifest->id);
            progress = true;
        }
    }

    std::vector<std::string> cycle;
    for (const PluginManifest* manifest : ready) {
        if (!placed.contains(manifest->id)) {
            cycle.push_back(manifest->id);
        }
    }
    if (!cycle.empty()) {
        std::sort(cycle.begin(), cycle.end());
        std::string names;
        for (const std::string& id : cycle) {
            if (!names.empty()) {
                names += ", ";
            }
            names += id;
        }
        for (const std::string& id : cycle) {
            refuse(refusals, id, "it is in a dependency cycle with: " + names);
        }
    }

    std::vector<PluginOrdering> out;
    for (const PluginManifest* manifest : ordered) {
        out.push_back(PluginOrdering{.manifest = *manifest, .refusal = {}});
    }
    std::vector<const PluginManifest*> byName;
    for (const PluginManifest& manifest : candidates) {
        byName.push_back(&manifest);
    }
    std::sort(byName.begin(), byName.end(),
              [](const PluginManifest* left, const PluginManifest* right) { return left->id < right->id; });
    for (const PluginManifest* manifest : byName) {
        const auto reason = refusals.find(manifest->id);
        if (reason == refusals.end()) {
            continue;
        }
        out.push_back(PluginOrdering{.manifest = *manifest, .refusal = reason->second});
    }
    return out;
}

std::vector<std::string> pluginOrder::dependents(std::string_view id, const std::vector<PluginManifest>& loaded) {
    std::vector<std::string> found;
    std::vector<std::string> frontier{std::string{id}};
    while (!frontier.empty()) {
        std::vector<std::string> next;
        for (const PluginManifest& manifest : loaded) {
            const bool depends = std::any_of(manifest.dependencies.begin(), manifest.dependencies.end(),
                                             [&](const PluginDependency& dependency) {
                                                 return dependency.hard && std::find(frontier.begin(), frontier.end(),
                                                                                     dependency.id) != frontier.end();
                                             });
            if (!depends) {
                continue;
            }
            if (manifest.id == id || std::find(found.begin(), found.end(), manifest.id) != found.end()) {
                continue;
            }
            next.push_back(manifest.id);
        }
        if (next.empty()) {
            break;
        }
        found.insert(found.end(), next.begin(), next.end());
        frontier = std::move(next);
    }
    std::reverse(found.begin(), found.end());
    return found;
}
} // namespace basalt::wire::plugins
