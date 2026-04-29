$ErrorActionPreference = 'Stop'

$BuildDir = if ($args.Count -gt 0) { $args[0] } else { 'build' }
$ToolRoot = 'E:\toolchains'

$Zig = Get-ChildItem -LiteralPath $ToolRoot -Directory -Filter 'zig-*' -ErrorAction SilentlyContinue |
    Where-Object { Test-Path (Join-Path $_.FullName 'zig.exe') } |
    Sort-Object LastWriteTimeUtc -Descending |
    Select-Object -First 1

$CMakeArgs = @('-S', '.', '-B', $BuildDir, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release')
if ($Zig) {
    $CMakeArgs += "-DZIG_EXECUTABLE=$((Join-Path $Zig.FullName 'zig.exe').Replace('\', '/'))"
}
if (Test-Path 'E:\toolchains\cargo\bin\cargo.exe') {
    $CMakeArgs += '-DCARGO_EXECUTABLE=E:/toolchains/cargo/bin/cargo.exe'
    $CMakeArgs += '-DRUSTUP_HOME_DIR=E:/toolchains/rustup'
    $CMakeArgs += '-DCARGO_HOME_DIR=E:/toolchains/cargo'
}
if (Test-Path 'E:\v_go_ds50_benchmark\tools\v\v.exe') {
    $CMakeArgs += '-DV_EXECUTABLE=E:/v_go_ds50_benchmark/tools/v/v.exe'
}

cmake @CMakeArgs
cmake --build $BuildDir --config Release
