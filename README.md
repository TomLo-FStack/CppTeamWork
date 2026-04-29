# CppTeamWork System Language Expense Lab

This is a full rewrite of the original single-file student expense tracker.
The old implementation is preserved in `legacy/main.original.cpp`; the new
code separates the work across five system-level languages behind a C ABI.

## Language Split

- C (`src/c`): date validation, amount parsing, status codes, stable ABI helpers.
- C++ (`src/cpp`): command-line and interactive controller.
- Zig (`src/zig`): monthly and category aggregation over ABI records.
- Rust (`src/rust`): ledger persistence, atomic writes, delete, settlement state.
- V (`src/v`): text normalization and canonical category formatting.

## Toolchains Used

- Zig: `0.17.0-dev.135+9df02121d` from the official Zig download index.
- Rust: stable GNU toolchain, `rustc 1.95.0 (59807616e 2026-04-14)`.
- V: `E:\v_go_ds50_benchmark\tools\v\v.exe`, version `0.5.1`.
- C/C++: MinGW GCC/G++ `15.2.0`, CMake, Ninja.

Run `scripts/bootstrap_toolchains.ps1` to redownload Zig and Rust into
`E:\toolchains`. Toolchain binaries are intentionally not committed.

## Build

```powershell
.\scripts\build.ps1
```

The main executable is `build\expense_app.exe`.

## Usage

```powershell
build\expense_app.exe --data expenses.etx add 2026-04-01 12.50 food "campus lunch"
build\expense_app.exe --data expenses.etx list month 2026-04
build\expense_app.exe --data expenses.etx summary 2026-04
build\expense_app.exe --data expenses.etx delete 1
build\expense_app.exe tui
```

## Verification

```powershell
.\tests\smoke.ps1 build
```
