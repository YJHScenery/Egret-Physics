<# @file setup_control_math.ps1 @brief 准备固定版本的预编译数学库，校验官方归档；后续运行仅链接已有库。 #>
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$archive=Join-Path $repo '.deps_download/control/wasi-sysroot-27.0.tar.gz'
$deps=Join-Path $repo 'dependency/control'
New-Item -ItemType Directory -Force (Split-Path $archive),$deps | Out-Null
$hash='7110ac48f5d0b1f6ab67d57aecf52450540dddd790cafdc45f0fdfb429bdab84'
if(!(Test-Path $archive)){Invoke-WebRequest 'https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-27/wasi-sysroot-27.0.tar.gz' -OutFile $archive}
if((Get-FileHash $archive -Algorithm SHA256).Hash -ne $hash){throw 'WASI SDK 数学库归档摘要不匹配'}
& tar -xzf $archive -C $deps
if($LASTEXITCODE -ne 0){throw 'WASI SDK 解包失败'}
# 仅按需链接公开数学符号；不启用 WASI 导入，不向用户开放其他 libc 接口。
Copy-Item (Join-Path $deps 'wasi-sysroot-27.0/lib/wasm32-wasi/libc.a') (Join-Path $repo 'src/control_core/sdk/libegret_math.a') -Force
Write-Output '预编译数学库已准备，数学源码无需在用户编译时重编译。'
