$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$ToolRoot = 'E:\toolchains'
$DownloadRoot = Join-Path $ToolRoot 'downloads'
New-Item -ItemType Directory -Force -Path $DownloadRoot | Out-Null

$zigIndex = Invoke-RestMethod 'https://ziglang.org/download/index.json'
$zigVersion = $zigIndex.master.version
$zig = $zigIndex.master.'x86_64-windows'
$zigZip = Join-Path $DownloadRoot ("zig-$zigVersion-x86_64-windows.zip")
Invoke-WebRequest -UseBasicParsing -Uri $zig.tarball -OutFile $zigZip
$hash = (Get-FileHash -Algorithm SHA256 $zigZip).Hash.ToLowerInvariant()
if ($hash -ne $zig.shasum.ToLowerInvariant()) {
    throw "Zig checksum mismatch: $hash"
}
$zigDest = Join-Path $ToolRoot "zig-$zigVersion"
if (Test-Path $zigDest) {
    Remove-Item -LiteralPath $zigDest -Recurse -Force
}
$tmp = Join-Path $ToolRoot ('zig-extract-' + [Guid]::NewGuid().ToString('N'))
Expand-Archive -LiteralPath $zigZip -DestinationPath $tmp -Force
$folder = Get-ChildItem -LiteralPath $tmp -Directory | Select-Object -First 1
Move-Item -LiteralPath $folder.FullName -Destination $zigDest
Remove-Item -LiteralPath $tmp -Recurse -Force
& (Join-Path $zigDest 'zig.exe') version

$rustup = Join-Path $DownloadRoot 'rustup-init.exe'
Invoke-WebRequest -UseBasicParsing -Uri 'https://static.rust-lang.org/rustup/dist/x86_64-pc-windows-gnu/rustup-init.exe' -OutFile $rustup
$env:RUSTUP_HOME = Join-Path $ToolRoot 'rustup'
$env:CARGO_HOME = Join-Path $ToolRoot 'cargo'
& $rustup -y --no-modify-path --profile minimal --default-host x86_64-pc-windows-gnu --default-toolchain stable
& (Join-Path $env:CARGO_HOME 'bin\rustc.exe') --version
& (Join-Path $env:CARGO_HOME 'bin\cargo.exe') --version
