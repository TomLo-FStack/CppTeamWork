$ErrorActionPreference = 'Stop'

$BuildDir = if ($args.Count -gt 0) { $args[0] } else { 'build' }
cmake -S . -B $BuildDir -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DZIG_EXECUTABLE='E:/toolchains/zig-0.17.0-dev.135+9df02121d/zig.exe' `
    -DCARGO_EXECUTABLE='E:/toolchains/cargo/bin/cargo.exe' `
    -DRUSTUP_HOME_DIR='E:/toolchains/rustup' `
    -DCARGO_HOME_DIR='E:/toolchains/cargo' `
    -DV_EXECUTABLE='E:/v_go_ds50_benchmark/tools/v/v.exe'
cmake --build $BuildDir --config Release
