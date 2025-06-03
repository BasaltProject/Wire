<p align="center">
  <img src="docs/assets/basalt-logo.png" alt="Basalt" width="360">
</p>

<h2 align="center">Basalt Wire</h2>

<p align="center">
  <strong>The Minecraft network layer shared by Basalt Server and Basalt Proxy, in Kotlin/Native. No JVM.</strong>
</p>

<p align="center">
  <img alt="Minecraft" src="https://img.shields.io/badge/Minecraft-26.2-62B47A?logo=minecraft&logoColor=white">
  <img alt="Protocol" src="https://img.shields.io/badge/protocol-776-62B47A">
  <img alt="Kotlin" src="https://img.shields.io/badge/Kotlin-2.4.10-7F52FF?logo=kotlin&logoColor=white">
  <img alt="Kotlin/Native" src="https://img.shields.io/badge/Kotlin%2FNative-macOS_%7C_Linux_%7C_Windows-7F52FF?logo=kotlin&logoColor=white">
  <img alt="Build" src="https://img.shields.io/badge/build-Gradle-02303A?logo=gradle&logoColor=white">
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

Everything here is written from scratch in Kotlin/Native. There is no JVM in the
runtime and no Java library in the build: no Netty, no Adventure, no Gson.

| | |
|---|---|
| Language | Kotlin/Native, 100 %. No Java, no JVM at runtime |
| Target version | Minecraft 26.2 (protocol 776) |
| Message model | Adventure-style immutable components; no legacy `§` formatting |
| Consumed as | A git submodule, composed into the consumer's Gradle build |
| License | GPL-3.0 |

## Modules

Dependencies point downwards only: `basalt-protocol` knows about `basalt-nbt`,
never the other way round.

| Module | What it holds |
|---|---|
| `basalt-common` | Logging, primitive collections, hashing, identifiers, timings. Depends on nothing |
| `basalt-nbt` | NBT and SNBT, reading and writing |
| `basalt-text` | Adventure-style components, styles, JSON and NBT serialisation |
| `basalt-io` | Sockets, event loop, byte buffers, zlib, gzip, zstd |
| `basalt-protocol` | Packet framing, varints, and the codecs for every protocol state |
| `basalt-auth` | Encrypted handshake, Mojang session lookup, profiles, offline UUIDs |
| `basalt-multiversion` | Translation for older Java clients |
| `basalt-bedrock` | RakNet and Bedrock protocol translation |

Everything above the network layer, worlds, entities, gameplay, plugins, lives
in the consuming repository instead.

## Using it

Add the submodule, then compose it into the Gradle build.

```bash
git submodule add https://github.com/BasaltProject/Wire.git wire
```

```kotlin
// settings.gradle.kts
pluginManagement {
    includeBuild("wire/build-logic")
}

dependencyResolutionManagement {
    versionCatalogs {
        create("libs") { from(files("wire/gradle/libs.versions.toml")) }
    }
}

include(":basalt-protocol")
project(":basalt-protocol").projectDir = file("wire/basalt-protocol")
```

Wire owns the shared build convention (`basalt.native-module`) and the version
catalog, so consumers inherit one Kotlin version, one ktlint configuration and
one set of native targets rather than keeping their own in sync.

Clone a consumer with `--recurse-submodules`, or run
`git submodule update --init` in an existing clone.

## Building

Gradle orchestrates the build; it is not part of the runtime.

Kotlin/Native tasks carry the target in their name; replace `MacosArm64` with the
one you are building for.

```bash
# Compile, style and test everything on this host
./gradlew build

# A single module
./gradlew :basalt-protocol:macosArm64Test
```

Supported targets: `macosArm64`, `macosX64`, `linuxX64`, `linuxArm64`,
`mingwX64`. Kotlin/Native does not cross-compile reliably between host families,
so a build registers only the targets its own host can produce: macOS builds the
two macOS targets, Linux the two Linux ones, Windows `mingwX64`. CI covers all
five across three runners.

## Testing

Every packet codec carries a round-trip test, and every binary format is checked
against known vectors. The vectors are generated synthetically: no proprietary
Minecraft file is committed here, extracted here, or distributed here.

Native tests only run on their matching host.

## Requirements

| Component | Requirement |
|---|---|
| JDK | 25, for Gradle only. Not part of the runtime |
| Kotlin | 2.4.10 |
| Gradle | Through the included wrapper |
| Native toolchain | Required to link or run Kotlin/Native binaries on the host |

Use the included wrapper rather than a system Gradle installation.

## Development Status

| Module | Status |
|---|---|
| `basalt-common` | Working |
| `basalt-nbt` | Working |
| `basalt-text` | Working |
| `basalt-io` | Working |
| `basalt-protocol` | Handshake, status, login, configuration and play |
| `basalt-auth` | Working: encryption, Mojang session, profiles |
| `basalt-multiversion` | Not started |
| `basalt-bedrock` | Not started |

Codecs are written against protocol 776 and are not yet parameterised by
version. Making them so is the work `basalt-multiversion` depends on.

## Related repositories

| Repository | What it is |
|---|---|
| [Server](https://github.com/BasaltProject/Server) | The Minecraft server itself, in Kotlin/Native |
| [Proxy](https://github.com/BasaltProject/Proxy) | The proxy component |
| [Website](https://github.com/BasaltProject/Website) | The project site, at [basaltmc.org](https://basaltmc.org) |

## License

Basalt Wire is distributed under the [GNU General Public License v3.0](LICENSE).

Basalt is not affiliated with Mojang Studios or Microsoft. Minecraft is a
trademark of Mojang Studios.
