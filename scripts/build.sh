#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-build}"
tool_root="${CPPTEAMWORK_TOOL_ROOT:-$HOME/.cppteamwork-toolchains}"

cmake_args=(-S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Release)
if command -v ninja >/dev/null 2>&1; then
    cmake_args+=(-G Ninja)
fi

if [[ -n "${ZIG_EXECUTABLE:-}" ]]; then
    cmake_args+=("-DZIG_EXECUTABLE=$ZIG_EXECUTABLE")
else
    zig_candidate="$(find "$tool_root" -maxdepth 2 -type f -name zig 2>/dev/null | sort -r | head -n 1 || true)"
    if [[ -n "$zig_candidate" ]]; then
        cmake_args+=("-DZIG_EXECUTABLE=$zig_candidate")
    fi
fi

if [[ -x "$tool_root/cargo/bin/cargo" ]]; then
    cmake_args+=("-DCARGO_EXECUTABLE=$tool_root/cargo/bin/cargo")
    cmake_args+=("-DRUSTUP_HOME_DIR=$tool_root/rustup")
    cmake_args+=("-DCARGO_HOME_DIR=$tool_root/cargo")
elif command -v cargo >/dev/null 2>&1; then
    cmake_args+=("-DCARGO_EXECUTABLE=$(command -v cargo)")
fi

if [[ -n "${V_EXECUTABLE:-}" ]]; then
    cmake_args+=("-DV_EXECUTABLE=$V_EXECUTABLE")
elif [[ -x "$tool_root/v/v" ]]; then
    cmake_args+=("-DV_EXECUTABLE=$tool_root/v/v")
elif command -v v >/dev/null 2>&1; then
    cmake_args+=("-DV_EXECUTABLE=$(command -v v)")
fi

cmake "${cmake_args[@]}"
cmake --build "$build_dir" --config Release
