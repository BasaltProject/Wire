#include <basalt/wire/plugins/PluginFailure.hpp>

namespace basalt::wire::plugins {
bool PluginFailure::outOfBudget() const {
    return message.find(failureMessages::kBudget) != std::string::npos ||
           message.find(failureMessages::kInterrupted) != std::string::npos;
}

bool PluginFailure::outOfMemory() const { return message.find(failureMessages::kMemory) != std::string::npos; }
} // namespace basalt::wire::plugins
