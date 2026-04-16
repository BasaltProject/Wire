# Basalt threading adapter

The vendored Mbed TLS library uses its upstream pthread implementation on POSIX
and its upstream alternate-threading interface with native SRW locks on Windows.
The adapter is Basalt code; upstream library sources and notices remain intact.

Configuration definitions are public because mutex fields affect crypto-context
layout. Native callers initialize the Windows callbacks once before creating
contexts; no Java runtime or external pthread dependency is used on Windows.
The global SRW locks have process lifetime and do not allocate OS handles.

The configuration was added after concurrent TLS integration tests exposed a
race in global PSA key slots under ThreadSanitizer with threading disabled.
