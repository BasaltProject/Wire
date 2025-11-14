#pragma once

#include <string>
#include <string_view>

namespace basalt::wire::common::panic {

/// Turns a crash into a report instead of one line from the shell: catches the fatal signals and
/// uncaught exceptions, prints what happened and where, then lets the default handler run so the
/// exit status is unchanged.
///
/// `reportUrl` is a parameter because this module is shared: Wire has no business knowing whether
/// the server or the proxy linked it. Empty asks for a report without saying where to send it.
void install(std::string name, std::string version, std::string reportUrl = {});

/// Prints the report for something the process decided is fatal but no signal announced.
///
/// The same shape as a crash report, for the cases a bad configuration or a failed bind wants to
/// end the process loudly rather than quietly.
void report(std::string_view title, std::string_view detail);

} // namespace basalt::wire::common::panic
