#!/usr/bin/env bash
set -euo pipefail

tool_root="${CPPTEAMWORK_TOOL_ROOT:-$HOME/.cppteamwork-toolchains}"
download_root="$tool_root/downloads"
mkdir -p "$download_root"

case "$(uname -m)" in
    arm64|aarch64)
        zig_platform="aarch64-macos"
        ;;
    x86_64)
        zig_platform="x86_64-macos"
        ;;
    *)
        echo "unsupported macOS architecture: $(uname -m)" >&2
        exit 1
        ;;
esac

read -r zig_version zig_url zig_sha <<EOF
$(curl -fsSL 'https://ziglang.org/download/index.json' | python3 -c "import json,sys; data=json.load(sys.stdin); z=data['master']; p=z['$zig_platform']; print(z['version'], p['tarball'], p['shasum'])")
EOF

zig_archive="$download_root/zig-$zig_version-$zig_platform.tar.xz"
curl -fL "$zig_url" -o "$zig_archive"
actual_sha="$(shasum -a 256 "$zig_archive" | awk '{print $1}')"
if [[ "$actual_sha" != "$zig_sha" ]]; then
    echo "Zig checksum mismatch: $actual_sha" >&2
    exit 1
fi

zig_dest="$tool_root/zig-$zig_version"
rm -rf "$zig_dest"
tmp_dir="$(mktemp -d "$tool_root/zig-extract.XXXXXX")"
tar -xf "$zig_archive" -C "$tmp_dir"
extracted="$(find "$tmp_dir" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
mv "$extracted" "$zig_dest"
rm -rf "$tmp_dir"
"$zig_dest/zig" version

curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs |
    RUSTUP_HOME="$tool_root/rustup" CARGO_HOME="$tool_root/cargo" \
    sh -s -- -y --no-modify-path --profile minimal --default-toolchain stable
"$tool_root/cargo/bin/rustc" --version
"$tool_root/cargo/bin/cargo" --version

if [[ ! -x "$tool_root/v/v" ]]; then
    rm -rf "$tool_root/v"
    git clone --depth=1 https://github.com/vlang/v "$tool_root/v"
else
    git -C "$tool_root/v" pull --ff-only
fi
make -C "$tool_root/v"
"$tool_root/v/v" version
