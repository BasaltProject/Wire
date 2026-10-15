#pragma once

#include <string>
#include <string_view>

namespace basalt::wire::common::panic {

void install(std::string name, std::string version, std::string reportUrl = {});

void report(std::string_view title, std::string_view detail);

} // namespace basalt::wire::common::panic
