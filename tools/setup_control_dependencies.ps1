<#
@file setup_control_dependencies.ps1
@brief 下载并核验固定版本控制依赖，LLVM 只解包不安装，不修改系统配置。
#>
param([Parameter(Mandatory=$true)][string]$ArchiveExtractor)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$deps=Join-Path $repo 'dependency/control'
$downloads=Join-Path $repo '.deps_download/control'
New-Item -ItemType Directory -Force $deps,$downloads | Out-Null
<# @brief 下载指定文件并核验固定摘要。 #>
function Get-VerifiedFile([string]$Url,[string]$Path,[string]$Hash){
    if(!(Test-Path -LiteralPath $Path)){Invoke-WebRequest $Url -OutFile $Path}
    if((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Hash){throw "文件摘要不匹配：$Path"}
}
$wasmtime=Join-Path $downloads 'wasmtime.zip'
$llvm=Join-Path $downloads 'llvm.exe'
Get-VerifiedFile 'https://github.com/bytecodealliance/wasmtime/releases/download/v38.0.3/wasmtime-v38.0.3-x86_64-windows-c-api.zip' $wasmtime '6427A2EEFEEB9DA7ABE717B313CE4D75DB9B910BC763D6D3C8B284C28812D7A3'
Get-VerifiedFile 'https://github.com/llvm/llvm-project/releases/download/llvmorg-21.1.8/LLVM-21.1.8-win64.exe' $llvm '7A5386C26497DB1691F320121E5B113364DD0274B98E55F15F4DBC00C0450113'
Expand-Archive -LiteralPath $wasmtime -DestinationPath $deps -Force
$llvmRoot=Join-Path $deps 'llvm'
& $ArchiveExtractor x $llvm "-o$llvmRoot" 'bin/clang.exe' 'bin/lld.exe' -y
if($LASTEXITCODE -ne 0){throw 'LLVM 解包失败'}
Copy-Item -LiteralPath (Join-Path $llvmRoot 'bin/lld.exe') -Destination (Join-Path $llvmRoot 'bin/wasm-ld.exe') -Force
Invoke-WebRequest 'https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-21.1.8/llvm/LICENSE.TXT' -OutFile (Join-Path $llvmRoot 'LICENSE.TXT')
Get-VerifiedFile 'https://raw.githubusercontent.com/nlohmann/json/v3.12.0/single_include/nlohmann/json.hpp' (Join-Path $deps 'json.hpp') 'AAF127C04CB31C406E5B04A63F1AE89369FCCDE6D8FA7CDDA1ED4F32DFC5DE63'
Get-VerifiedFile 'https://raw.githubusercontent.com/nlohmann/json/v3.12.0/LICENSE.MIT' (Join-Path $deps 'json-LICENSE.MIT') '46A65CFFD1EA955132D95A8DD921640714A8D6B537D2E4E482D31145AE95B603'
$lua=Join-Path $downloads 'lua-5.4.8.tar.gz'
Get-VerifiedFile 'https://www.lua.org/ftp/lua-5.4.8.tar.gz' $lua '4F18DDAE154E793E46EEAB727C59EF1C0C0C2B744E7B94219710D76F530629AE'
& tar -xzf $lua -C $deps
if($LASTEXITCODE -ne 0){throw 'Lua 解包失败'}
Write-Output '控制依赖准备完成。解包工具仅用于开发准备，不随产品分发。'

& (Join-Path $PSScriptRoot 'setup_control_math.ps1')
