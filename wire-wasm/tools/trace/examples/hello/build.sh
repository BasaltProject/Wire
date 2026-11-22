#!/bin/sh
set -e
here=$(dirname "$0")
clang --target=wasm32 -O2 -nostdlib -fno-builtin -I"$here/../../../../abi" -Wl,--no-entry -Wl,--export-memory \
    -Wl,--allow-undefined -o "$here/plugin.wasm" "$here/src/hello.c"
