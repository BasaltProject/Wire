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
  <strong>The Minecraft network layer shared by Basalt Server and Basalt Proxy, in C++.</strong>
</p>

<p align="center">
  <img alt="Java Edition" src="https://img.shields.io/badge/Java_Edition-26.3-62B47A?logo=minecraft&logoColor=white">
  <img alt="Bedrock Edition" src="https://img.shields.io/badge/Bedrock_Edition-26.60-62B47A?logo=minecraft&logoColor=white">
  <img alt="C++" src="https://img.shields.io/badge/language-C%2B%2B-00599C?logo=cplusplus&logoColor=white">
  <img alt="Platforms" src="https://img.shields.io/badge/platforms-macOS_%7C_Linux_%7C_Windows-00599C">
  <img alt="License" src="https://img.shields.io/badge/license-GPL--3.0-green">
</p>

## About

Wire is everything Basalt needs to speak Minecraft over a socket, and nothing else: packet framing and codecs, NBT,
the component model, the encrypted login handshake, and translation for older Java clients and for Bedrock.

It is not a standalone program. [Server](https://github.com/BasaltProject/Server) and
[Proxy](https://github.com/BasaltProject/Proxy) both consume it as a git submodule, so there is one implementation of
the protocol to keep correct.

## Modules

| Module | What it holds |
|---|---|
| `wire-common` | Logging, collections, hashing, identifiers. Depends on nothing |
| `wire-nbt` | NBT and SNBT |
| `wire-text` | Adventure-style components and their JSON and NBT forms |
| `wire-io` | Sockets, event loop, buffers, compression |
| `wire-protocol` | Packet framing and the codecs for every protocol state |
| `wire-auth` | Encrypted handshake, Mojang sessions, profiles |
| `wire-multiversion` | Translation for older Java clients |
| `wire-bedrock` | NetherNet, RakNet, Bedrock sessions and codecs |
| `wire-plugins` | Plugin manifests, packages and shared plugin types |
| `wire-sqlite` | SQLite access, compiled into the binary |

## Using it

```bash
git submodule add https://github.com/BasaltProject/Wire.git wire
```

```cmake
add_subdirectory(wire)
target_link_libraries(your-module PUBLIC basalt::wire-protocol)
```

Wire owns the shared CMake functions and the third-party dependencies, so consumers inherit one set of flags and one
copy of every dependency.

## Building

```bash
cmake --preset debug
cmake --build build/debug
ctest --preset debug
```

Needs CMake 3.28+, Ninja and Clang.

## License

[GPL-3.0](LICENSE).

Basalt is not affiliated with Mojang Studios or Microsoft. Minecraft is a trademark of Mojang Studios.
