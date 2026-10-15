# c-ares

Version 1.34.8, MIT license; notices are retained in upstream/LICENSE.md and source files.
Imported from https://github.com/c-ares/c-ares/releases/download/v1.34.8/c-ares-1.34.8.tar.gz.
Archive SHA-256: c222b6d681096f9444d2c4863d2c1174019e27cacca0a4a5c114d36dd7d7bf78.

The retained upstream files are unchanged. Release packaging, test harnesses,
the private APSL Apple header and its adapter are omitted. The wrapper selects a
static library without installed dependencies, tools or upstream tests. Wire uses
its own readiness loop, not the optional c-ares event thread.

macOS uses a Basalt GPL-3.0-only adapter to public SystemConfiguration APIs for
default DNS servers and search domains, with the standard resolver file fallback.
Split/VPN resolver selection is not certified. No private Apple headers or APIs
are imported or dynamically loaded. This adapter still needs native macOS validation.
