# CppTeamWork System Language Expense Lab

This is a system-language expense tracker lab. The code separates the work
across five low-level languages behind a stable C ABI.

## Language Split

- C (`src/c`): date validation, amount parsing, status codes, stable ABI helpers.
- C++ (`src/cpp`): command-line and interactive controller.
- Zig (`src/zig`): monthly and category aggregation over ABI records.
- Rust (`src/rust`): ledger persistence, atomic writes, delete, settlement state.
- V (`src/v`): text normalization and canonical category formatting.

## Toolchains Used

- Zig: `0.17.0-dev.135+9df02121d` from the official Zig download index.
- Rust: stable GNU toolchain, `rustc 1.95.0 (59807616e 2026-04-14)`.
- V on Windows: `E:\v_go_ds50_benchmark\tools\v\v.exe`, version `0.5.1`.
- V on macOS: discovered from `~/.cppteamwork-toolchains/v/v` or `PATH`.
- C/C++: CMake plus GCC/Clang and Ninja or the default CMake generator.

Toolchain binaries are intentionally not committed.

Windows:

```powershell
.\scripts\bootstrap_toolchains.ps1
```

macOS:

```bash
bash scripts/bootstrap_toolchains.sh
```

The macOS bootstrap downloads the latest Zig master build from the official
download index, installs Rust stable through rustup, and builds V under
`~/.cppteamwork-toolchains`.

## Build

Windows:

```powershell
.\scripts\build.ps1
```

macOS:

```bash
bash scripts/build.sh
```

The main executable is `build\expense_app.exe` on Windows and
`build/expense_app` on macOS.

## Usage

```powershell
build\expense_app.exe --data expenses.etx add 2026-04-01 12.50 food "campus lunch"
build\expense_app.exe --data expenses.etx list month 2026-04
build\expense_app.exe --data expenses.etx summary 2026-04
build\expense_app.exe --data expenses.etx delete 1
build\expense_app.exe tui
```

macOS uses the same commands with `build/expense_app`.

## Verification

Windows:

```powershell
.\tests\smoke.ps1 build
```

macOS:

```bash
bash tests/smoke.sh build
```
