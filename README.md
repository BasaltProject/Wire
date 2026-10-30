<p align="center">
  <a href="https://raw.githubusercontent.com/BasaltProject/.github/main/assets/basalt-logo-white.svg#gh-dark-mode-only">
    <img src="https://raw.githubusercontent.com/BasaltProject/.github/main/assets/basalt-logo-white.svg" alt="Basalt" width="360">
  </a>
  <a href="https://raw.githubusercontent.com/BasaltProject/.github/main/assets/basalt-logo-black.svg#gh-light-mode-only">
    <img src="https://raw.githubusercontent.com/BasaltProject/.github/main/assets/basalt-logo-black.svg" alt="Basalt" width="360">
  </a>
</p>

<h2 align="center">Basalt Wire</h2>

<p align="center">
  <strong>The Minecraft network layer shared by Basalt Server and Basalt Proxy, in C++23. No JVM.</strong>
</p>

<p align="center">
  <img alt="Minecraft" src="https://img.shields.io/badge/Minecraft-26.3-62B47A?logo=minecraft&logoColor=white">
  <img alt="Protocol" src="https://img.shields.io/badge/protocol-777-62B47A">
  <img alt="C++" src="https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus&logoColor=white">
  <img alt="Platforms" src="https://img.shields.io/badge/platforms-macOS_%7C_Linux_%7C_Windows-00599C">
  <img alt="Build" src="https://img.shields.io/badge/build-CMake-064F8C?logo=cmake&logoColor=white">
  <img alt="License" src="https://img.shields.io/badge/license-GPL--3.0-green">
</p>

> **Pre-1.0.** Wire is consumed by Basalt Server and Basalt Proxy, which move together with it,
> so its APIs change without notice.

## Overview

Wire is everything Basalt needs to speak Minecraft over a socket, and nothing
else. Sockets and buffers, packet framing and codecs, NBT, the component model,
the encrypted login handshake, translation for older Java and Bedrock clients, and the Lua and plugin-package
plumbing both consumers share.

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
| Target version | Minecraft 26.3 (protocol 777) |
| Message model | Adventure-style immutable components; no legacy `§` formatting |
| Consumed as | A git submodule, added to the consumer's CMake build |
| License | GPL-3.0 |

## Modules

Dependencies point downwards only: `wire-protocol` knows about `wire-nbt`,
never the other way round. `wire-lua`, `wire-plugins` and `wire-sqlite` are shared infrastructure that
both consumers use and that holds no game state.

| Module | What it holds |
|---|---|
| `wire-common` | Logging, primitive collections, hashing, identifiers, timings. Depends on nothing |
| `wire-nbt` | NBT and SNBT, reading and writing |
| `wire-text` | Adventure-style components, styles, JSON and NBT serialisation |
| `wire-io` | Sockets, event loop, byte buffers, zlib, gzip, zstd |
| `wire-protocol` | Packet framing, varints, and the codecs for every protocol state |
| `wire-auth` | Encrypted handshake, Mojang session lookup, profiles, offline UUIDs |
| `wire-multiversion` | Translation chains for older Java clients |
| `wire-bedrock` | RakNet, Bedrock sessions and codecs, and data shared with the Bedrock translation |
| `wire-lua` | The sandboxed Lua 5.5 runtime bindings |
| `wire-plugins` | Plugin manifests, packages, signatures and shared plugin types |
| `wire-sqlite` | SQLite access and migrations, compiled into the binary |

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
| `wire-common`, `wire-nbt`, `wire-text`, `wire-io` | Working |
| `wire-protocol` | Handshake, status, login, configuration and play, at protocol 777 |
| `wire-auth` | Working: encryption, Mojang session, profiles |
| `wire-multiversion` | In use by Server for older Java clients; still maturing, and a link existing does not certify every client and backend pairing |
| `wire-bedrock` | RakNet, login, encryption, batching and codecs; part of the Java to Bedrock translation still lives in Server |
| `wire-lua`, `wire-plugins`, `wire-sqlite` | Working, used by Server |

Native codecs are written against protocol 777; older versions go through `wire-multiversion`.

## License

Basalt Wire is distributed under the [GNU General Public License v3.0](LICENSE).

Basalt is not affiliated with Mojang Studios or Microsoft. Minecraft is a
trademark of Mojang Studios.
