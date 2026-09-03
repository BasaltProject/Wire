<p align="center">
  <img src="docs/assets/basalt-logo.png" alt="Basalt" width="360">
</p>

<h2 align="center">Basalt Wire</h2>

<p align="center">
  <strong>The Minecraft network layer shared by Basalt Server and Basalt Proxy, in C++23. No JVM.</strong>
</p>

<p align="center">
  <img alt="Minecraft" src="https://img.shields.io/badge/Minecraft-26.2-62B47A?logo=minecraft&logoColor=white">
  <img alt="Protocol" src="https://img.shields.io/badge/protocol-776-62B47A">
  <img alt="C++" src="https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus&logoColor=white">
  <img alt="Platforms" src="https://img.shields.io/badge/platforms-macOS_%7C_Linux_%7C_Windows-00599C">
  <img alt="Build" src="https://img.shields.io/badge/build-CMake-064F8C?logo=cmake&logoColor=white">
  <img alt="License" src="https://img.shields.io/badge/license-GPL--3.0-green">
</p>

> ⚠️ **Early development.** Wire is consumed by two projects that are themselves
> under construction. Its APIs change without notice.

## Overview

Wire is everything Basalt needs to speak Minecraft over a socket, and nothing
else. Sockets and buffers, packet framing and codecs, NBT, the component model,
and the encrypted login handshake.

It is not a standalone program. It is the foundation shared by the two Basalt
components that talk to Minecraft clients:

| Repository | What it uses Wire for |
|---|---|
| [Server](https://github.com/BasaltProject/Server) | Speaking the protocol to connected players, and authenticating them |
| [Proxy](https://github.com/BasaltProject/Proxy) | Reading both sides of a connection, and authenticating players before handing them to a backend |

Both consume it as a git submodule, so a change to a packet lands in both at
once and there is only one implementation of the protocol to keep correct.

Everything here is written from scratch in C++23. There is no JVM in the
runtime and no Java library in the build: no Netty, no Adventure, no Gson.

| | |
|---|---|
| Language | C++23, 100 %. No Java, no JVM at runtime |
| Target version | Minecraft 26.2 (protocol 776) |
| Message model | Adventure-style immutable components; no legacy `§` formatting |
| Consumed as | A git submodule, added to the consumer's CMake build |
| License | GPL-3.0 |

## Modules

Dependencies point downwards only: `wire-protocol` knows about `wire-nbt`,
never the other way round.

| Module | What it holds |
|---|---|
| `wire-common` | Logging, primitive collections, hashing, identifiers, timings. Depends on nothing |
| `wire-nbt` | NBT and SNBT, reading and writing |
| `wire-text` | Adventure-style components, styles, JSON and NBT serialisation |
| `wire-io` | Sockets, event loop, byte buffers, zlib, gzip, zstd |
| `wire-protocol` | Packet framing, varints, and the codecs for every protocol state |
| `wire-auth` | Encrypted handshake, Mojang session lookup, profiles, offline UUIDs |
| `wire-multiversion` | Translation for older Java clients |
| `wire-bedrock` | RakNet and Bedrock protocol translation |

Everything above the network layer, worlds, entities, gameplay, plugins, lives
in the consuming repository instead.

## Using it

Add the submodule, then pull it into the CMake build.

```bash
git submodule add https://github.com/BasaltProject/Wire.git wire
```

```cmake
# CMakeLists.txt, before anything that links against a wire module
add_subdirectory(wire)

target_link_libraries(your-module PUBLIC basalt::wire-protocol)
```

Wire owns the shared build functions (`basalt_module`, `basalt_test`,
`basalt_bench`, the warnings, the sanitizers and `basalt::parity`) and the
vendored dependencies in `third_party/`, so consumers inherit one set of
compiler flags and one copy of every dependency rather than keeping their own in
sync.

Clone a consumer with `--recurse-submodules`, or run
`git submodule update --init` in an existing clone.

## Building

CMake 3.28 or newer, Ninja and Clang.

```bash
# Configure and build everything on this host
cmake --preset debug
cmake --build build/debug

# Test everything, or a single module
ctest --preset debug
./build/debug/wire-nbt/wire-nbt-test
```

Supported targets: macOS on arm64 and x64, Linux on x64 and arm64, and Windows
through mingw. Clang cross-compiles between them; CI covers all five across
three runners.

## Testing

Every packet codec carries a round-trip test, and every binary format is checked
against known vectors. The vectors are generated synthetically: no proprietary
Minecraft file is committed here, extracted here, or distributed here.

## Requirements

| Component | Requirement |
|---|---|
| CMake | 3.28 or newer |
| Generator | Ninja |
| Compiler | Clang, with C++23 support |
| ccache | Optional, but the build is much faster with it |

## Development Status

| Module | Status |
|---|---|
| `wire-common` | Working |
| `wire-nbt` | Working |
| `wire-text` | Working |
| `wire-io` | Working |
| `wire-protocol` | Handshake, status, login, configuration and play |
| `wire-auth` | Working: encryption, Mojang session, profiles |
| `wire-multiversion` | Not started |
| `wire-bedrock` | Not started |

Codecs are written against protocol 776 and are not yet parameterised by
version. Making them so is the work `wire-multiversion` depends on.

## Related repositories

| Repository | What it is |
|---|---|
| [Server](https://github.com/BasaltProject/Server) | The Minecraft server itself, in C++23 |
| [Proxy](https://github.com/BasaltProject/Proxy) | The proxy component |
| [Website](https://github.com/BasaltProject/Website) | The project site, at [basaltmc.org](https://basaltmc.org) |

## License

Basalt Wire is distributed under the [GNU General Public License v3.0](LICENSE).

Basalt is not affiliated with Mojang Studios or Microsoft. Minecraft is a
trademark of Mojang Studios.
