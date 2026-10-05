<#
.SYNOPSIS
    重建 Egret Physics 的 dependency 第三方依赖目录。

.DESCRIPTION
    dependency 目录中包含大量体积较大的第三方库与二进制文件，不随仓库分发。
    本脚本在没有现成 dependency 目录时（例如刚克隆仓库）按下列流程重建它：

        check     检测工具链（Visual Studio 2026 / MSVC、CMake、Ninja、Qt 6.9.3、curl、tar）
        fetch     从各库官方地址下载源码包 / 预编译包（支持断点续传与 SHA256 校验）
        headers   解压并整理纯头文件库：Boost、Eigen、magic_enum、CLI11
        ffmpeg    解压 gyan.dev 官方 shared 构建（9.0.2-full_build），整理 bin/lib/include/preset
        assimp    使用 MSVC x64 编译 Assimp 6.0.2（Release + Debug），产出 assimp-vc143-mt(d).dll/.lib
        qxlsx     使用 Qt 6.9.3 + MSVC 编译 QXlsx(Qt6) 1.5.0 静态库（Release + Debug）
        muparser  下载并整理 muparser 2.3.5 的头文件与核心源文件（随主程序编译）
        qcustomplot 下载并整理 QCustomPlot 2.1.1 的单头单源与 GPL 许可证（随主程序编译）
        assemble  把 work\stage 中整理好的文件发布为项目根目录下的 dependency
        verify    与 baseline\dependency_manifest.csv 比对，输出差异报告

    所有中间产物（下载缓存、解压源码、构建目录）统一放在 dependency_build\work 下，
    不会写入项目其它目录。

.PARAMETER Step
    要执行的步骤，可多选，取值：
    init | check | fetch | headers | ffmpeg | assimp | qxlsx | muparser | qcustomplot | build | assemble | verify | all | clean

.PARAMETER Force
    忽略已有缓存：重新下载、重新解压、重新编译。

.PARAMETER OutputDir
    发布目录，默认为 <项目根>\dependency。

.PARAMETER Jobs
    编译并行度，默认取逻辑处理器数量。

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File dependency_build\Build-Dependencies.ps1 -Step all

.EXAMPLE
    # 只重建头文件类依赖并发布
    powershell -NoProfile -ExecutionPolicy Bypass -File dependency_build\Build-Dependencies.ps1 -Step headers,ffmpeg,assemble

.NOTES
    作者：Egret Physics 开发辅助脚本
    依赖：Windows + PowerShell 5.1 及以上；联网环境；Visual Studio 2026（含 MSVC x64 工具链）；
          Qt 6.9.3 msvc2022_64（仅 QXlsx 需要，默认路径 D:\Qt\6.9.3\msvc2022_64）。
#>
[CmdletBinding()]
param(
    [string[]]$Step = @('all'),

    [switch]$Force,

    [string]$OutputDir,

    [string]$BaselineSource,

    [int]$Jobs = 0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# -Step 支持逗号分隔的多值写法（兼容使用 -File 调用时的参数传递方式）
$script:KnownSteps = @('init', 'baseline', 'check', 'fetch', 'headers', 'ffmpeg', 'assimp', 'qxlsx',
    'muparser', 'qcustomplot', 'build', 'assemble', 'verify', 'all', 'clean')
$Step = @(
    $Step | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ }
)
foreach ($s in $Step) {
    if ($script:KnownSteps -notcontains $s) {
        throw "未知步骤: '$s'。可选值: $($script:KnownSteps -join ', ')"
    }
}

# ------------------------------------------------------------------ 路径 --
$script:BuildRoot   = $PSScriptRoot                                   # dependency_build
$script:ProjectRoot = Split-Path -Parent $script:BuildRoot            # 项目根目录
$script:WorkRoot    = Join-Path $script:BuildRoot 'work'
$script:DownloadDir = Join-Path $script:WorkRoot   'downloads'
$script:SourceDir   = Join-Path $script:WorkRoot   'src'
$script:BuildDir    = Join-Path $script:WorkRoot   'build'
$script:StageDir    = Join-Path $script:WorkRoot   'stage'
$script:LogDir      = Join-Path $script:WorkRoot   'logs'
$script:ConfigDir   = Join-Path $script:BuildRoot  'config'
$script:ScriptsDir  = Join-Path $script:BuildRoot  'scripts'
$script:BaselineDir = Join-Path $script:BuildRoot  'baseline'
$script:LockFile    = Join-Path $script:WorkRoot   'sources.lock.json'

if ($OutputDir) { $script:OutputDir = $OutputDir } else { $script:OutputDir = Join-Path $script:ProjectRoot 'dependency' }

$script:WorkspaceDirs = @(
    $script:WorkRoot, $script:DownloadDir, $script:SourceDir,
    $script:BuildDir, $script:StageDir, $script:LogDir, $script:ConfigDir, $script:ScriptsDir, $script:BaselineDir
)

function Initialize-Workspace {
    <#
    .SYNOPSIS
        创建 dependency_build 下的工作目录骨架。
    #>
    foreach ($d in $script:WorkspaceDirs) {
        if (-not (Test-Path -LiteralPath $d)) {
            New-Item -ItemType Directory -Force -Path $d | Out-Null
        }
    }
}

function Write-Head {
    <#
    .SYNOPSIS
        输出步骤标题。
    #>
    param([string]$Text)
    Write-Host ''
    Write-Host ('=' * 78) -ForegroundColor DarkCyan
    Write-Host " $Text" -ForegroundColor Cyan
    Write-Host ('=' * 78) -ForegroundColor DarkCyan
}

# ------------------------------------------------------------------ 主流程 --
Initialize-Workspace

# 载入公共库与各步骤实现（首次运行 init 时它们可能尚不存在）
$commonFile = Join-Path $script:ScriptsDir 'Common.ps1'
if (Test-Path -LiteralPath $commonFile) { . $commonFile }

$stepFiles = @(
    'Steps.Fetch.ps1', 'Steps.Headers.ps1', 'Steps.FFmpeg.ps1', 'Steps.Assimp.ps1',
    'Steps.QXlsx.ps1', 'Steps.MuParser.ps1', 'Steps.QCustomPlot.ps1',
    'Steps.Assemble.ps1', 'Steps.Verify.ps1', 'Steps.Baseline.ps1'
)
foreach ($f in $stepFiles) {
    $p = Join-Path $script:ScriptsDir $f
    if (Test-Path -LiteralPath $p) { . $p }
}

function Resolve-StepList {
    <#
    .SYNOPSIS
        把用户传入的步骤集合展开为按序执行的步骤列表。
    #>
    param([string[]]$Requested)
    $order = @('check', 'fetch', 'headers', 'ffmpeg', 'assimp', 'qxlsx', 'muparser', 'qcustomplot', 'assemble', 'verify')
    $result = New-Object System.Collections.Generic.List[string]
    foreach ($s in $Requested) {
        switch ($s) {
            'all'   { foreach ($x in $order) { if (-not $result.Contains($x)) { $result.Add($x) } } }
            'build' { foreach ($x in @('check', 'fetch', 'headers', 'ffmpeg', 'assimp', 'qxlsx', 'muparser', 'qcustomplot')) { if (-not $result.Contains($x)) { $result.Add($x) } } }
            default { foreach ($x in @($s)) { if (-not $result.Contains($x)) { $result.Add($x) } } }
        }
    }
    return $result
}

function Invoke-DependencyStep {
    <#
    .SYNOPSIS
        执行单个重建步骤。
    #>
    param([string]$Name)
    switch ($Name) {
        'init'     { Write-Head '初始化工作目录';  Write-Ok "工作目录就绪: $script:WorkRoot" }
        'baseline' { Invoke-StepBaseline }
        'check'    { Invoke-StepCheckToolchain }
        'fetch'    { Invoke-StepFetch }
        'headers'  { Invoke-StepHeaders }
        'ffmpeg'   { Invoke-StepFFmpeg }
        'assimp'   { Invoke-StepAssimp }
        'qxlsx'    { Invoke-StepQXlsx }
        'muparser' { Invoke-StepMuParser }
        'qcustomplot' { Invoke-StepQCustomPlot }
        'assemble' { Invoke-StepAssemble }
        'verify'   { Invoke-StepVerify }
        'clean'    { Remove-WorkDirectory }
        default    { throw "未知步骤: $Name" }
    }
}

function Remove-WorkDirectory {
    <#
    .SYNOPSIS
        清理 dependency_build\work 下的全部中间产物（下载、解压、构建目录）。
    .NOTES
        仅删除工作目录，已发布到 dependency 的内容不受影响。
    #>
    if (-not (Test-Path -LiteralPath $script:WorkRoot)) { Write-Warn2 '工作目录不存在，无需清理'; return }
    $resolvedWork = (Resolve-Path -LiteralPath $script:WorkRoot).Path
    $resolvedRoot = (Resolve-Path -LiteralPath $script:BuildRoot).Path
    if (-not $resolvedWork.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "拒绝删除工作目录之外的路径: $resolvedWork"
    }
    Write-Head "清理中间产物: $resolvedWork"
    Remove-Item -LiteralPath $resolvedWork -Recurse -Force
    Write-Ok '清理完成'
}

$steps = Resolve-StepList $Step
Write-Host ''
Write-Host "项目根目录 : $script:ProjectRoot" -ForegroundColor DarkGray
Write-Host "工作目录   : $script:WorkRoot" -ForegroundColor DarkGray
Write-Host "发布目录   : $script:OutputDir" -ForegroundColor DarkGray
Write-Host "执行步骤   : $($steps -join ', ')" -ForegroundColor DarkGray

foreach ($s in $steps) {
    if ($s -in @('init', 'clean')) { Invoke-DependencyStep $s; continue }
    if (-not (Test-Path -LiteralPath $commonFile)) {
        throw "缺少 $commonFile，无法执行步骤 '$s'。请先补齐 dependency_build\scripts 下的脚本。"
    }
    Invoke-DependencyStep $s
}

Write-Host ''
Write-Host '全部步骤执行结束。' -ForegroundColor Green
