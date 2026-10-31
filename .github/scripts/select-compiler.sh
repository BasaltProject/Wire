#!/usr/bin/env bash
set -euo pipefail

case "$(uname -s)" in
Linux)
    wget -qO /tmp/llvm.sh https://apt.llvm.org/llvm.sh
    sudo bash /tmp/llvm.sh 19
    sudo apt-get install -y clang-format-19
    echo "BASALT_CMAKE_ARGS=-DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19" >>"$GITHUB_ENV"
    ;;
Darwin)
    brew install llvm
    llvm="$(brew --prefix llvm)"
    target=13.0
    echo "MACOSX_DEPLOYMENT_TARGET=$target" >>"$GITHUB_ENV"
    cross=""
    if [ "${BASALT_TARGET_ARCH:-}" = x86_64 ] && [ "$(uname -m)" = arm64 ]; then
        sudo softwareupdate --install-rosetta --agree-to-license || true
        cross="-DCMAKE_OSX_ARCHITECTURES=x86_64"
    fi
    echo "BASALT_CMAKE_ARGS=-DCMAKE_C_COMPILER=$llvm/bin/clang -DCMAKE_CXX_COMPILER=$llvm/bin/clang++ -DCMAKE_OSX_DEPLOYMENT_TARGET=$target -DCMAKE_CXX_FLAGS=-D_LIBCPP_DISABLE_AVAILABILITY $cross" >>"$GITHUB_ENV"
    ;;
*)
    echo "BASALT_CMAKE_ARGS=" >>"$GITHUB_ENV"
    ;;
esac
