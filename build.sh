#!/usr/bin/env bash
# Build script for Outpost 13-C.
# Run from the project root inside WSL2.
# Usage: bash build.sh [linux|windows|all]  (default: linux)
set -e
mkdir -p build

SDL_PREFIX="libs/SDL2-mingw/x86_64-w64-mingw32"

linux_build() {
    g++ -std=c++17 src/*.cpp -o build/outpost13c \
        -lSDL2 -lSDL2_image -lSDL2_mixer -lSDL2_ttf
    echo "Linux build complete: build/outpost13c"
}

windows_build() {
    if ! command -v x86_64-w64-mingw32-g++ &>/dev/null; then
        echo "ERROR: MinGW cross-compiler not found."
        echo "  sudo apt install mingw-w64"
        exit 1
    fi

    if [[ ! -d "${SDL_PREFIX}/include/SDL2" ]]; then
        echo "ERROR: MinGW SDL2 dev files not found at ${SDL_PREFIX}/"
        echo "  1. Download SDL2-devel-X.X.X-mingw.tar.gz from https://libsdl.org"
        echo "  2. Extract and place so that ${SDL_PREFIX}/include/SDL2/SDL.h exists."
        exit 1
    fi

    x86_64-w64-mingw32-g++ -std=c++17 src/*.cpp -o build/outpost13c.exe \
        -I"${SDL_PREFIX}/include/SDL2" \
        -L"${SDL_PREFIX}/lib" \
        -lmingw32 -lSDL2main -lSDL2 \
        -mconsole \
        -static-libgcc -static-libstdc++
    # Phase 3+ — add -lSDL2_image -lSDL2_mixer -lSDL2_ttf when those subsystems are used.

    cp -u "${SDL_PREFIX}/bin/SDL2.dll" build/
    echo "Windows build complete: build/outpost13c.exe"
}

case "${1:-linux}" in
    linux)   linux_build ;;
    windows) windows_build ;;
    all)     linux_build; windows_build ;;
    *)       echo "Usage: $0 [linux|windows|all]"; exit 1 ;;
esac
