<#
.SYNOPSIS
    dependency_build 公共函数库。

.DESCRIPTION
    提供下载、解压、文件整理、工具链探测、MSVC 环境初始化等公共能力，
    供 Build-Dependencies.ps1 与各 Steps.*.ps1 复用。
#>

# ------------------------------------------------------------------ 日志 --
$script:Toolchain = $null          # Get-Toolchain 的探测结果缓存

function Add-BuildLog {
    <#
    .SYNOPSIS
        追加一行构建日志到 work\logs\build.log。
    #>
    param([string]$Text)
    try {
        if (-not (Test-Path -LiteralPath $script:LogDir)) { New-Item -ItemType Directory -Force -Path $script:LogDir | Out-Null }
        $stamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
        Add-Content -LiteralPath (Join-Path $script:LogDir 'build.log') -Value "$stamp  $Text" -Encoding UTF8
    } catch { }
}

function Write-Info {
    <# .SYNOPSIS 普通信息。 #>
    param([string]$Text)
    Write-Host "[信息] $Text" -ForegroundColor Gray
}

function Write-Ok {
    <# .SYNOPSIS 成功信息。 #>
    param([string]$Text, [switch]$NoLog)
    Write-Host "[完成] $Text" -ForegroundColor Green
    if (-not $NoLog) { Add-BuildLog "OK    $Text" }
}

function Write-Warn2 {
    <# .SYNOPSIS 警告信息。 #>
    param([string]$Text)
    Write-Host "[警告] $Text" -ForegroundColor Yellow
    Add-BuildLog "WARN  $Text"
}

function Write-Fail2 {
    <# .SYNOPSIS 失败信息。 #>
    param([string]$Text)
    Write-Host "[失败] $Text" -ForegroundColor Red
    Add-BuildLog "ERROR $Text"
}

# ------------------------------------------------------------ 配置与工具链 --
function Get-DepConfig {
    <#
    .SYNOPSIS
        读取 config\dependencies.psd1 配置。
    #>
    $p = Join-Path $script:ConfigDir 'dependencies.psd1'
    if (-not (Test-Path -LiteralPath $p)) { throw "缺少配置文件: $p" }
    Import-PowerShellDataFile -LiteralPath $p
}

function Find-ExistingPath {
    <#
    .SYNOPSIS
        返回候选路径中第一个存在者，全部不存在时返回 $null。
    #>
    param([string[]]$Candidates)
    foreach ($c in $Candidates) {
        if ($c -and (Test-Path -LiteralPath $c)) { return $c }
    }
    return $null
}

function Find-Executable {
    <#
    .SYNOPSIS
        在候选路径与 PATH 中查找可执行文件。
    #>
    param([string[]]$Candidates, [string]$Name)
    $hit = Find-ExistingPath $Candidates
    if ($hit) { return $hit }
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}

function Get-Toolchain {
    <#
    .SYNOPSIS
        探测并缓存本机工具链路径（Visual Studio、CMake、Ninja、tar、curl、Qt）。
    #>
    if ($script:Toolchain) { return $script:Toolchain }

    $tc = @{}

    # ---- Visual Studio / MSVC ----
    $vsCandidates = @(
        'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
        'C:\Program Files\Microsoft Visual Studio\18\Professional\Common7\Tools\VsDevCmd.bat'
        'C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\Tools\VsDevCmd.bat'
        'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat'
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat'
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
    )
    $tc.VisualStudio = Find-ExistingPath $vsCandidates
    if (-not $tc.VisualStudio) {
        $vswhere = Find-ExistingPath @(
            (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe')
            'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
        )
        if ($vswhere) {
            $install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
            if ($install) {
                $cand = Join-Path $install.Trim() 'Common7\Tools\VsDevCmd.bat'
                if (Test-Path -LiteralPath $cand) { $tc.VisualStudio = $cand }
            }
        }
    }
    $vsIde = if ($tc.VisualStudio) { Split-Path -Parent (Split-Path -Parent $tc.VisualStudio) } else { $null }

    # ---- CMake / Ninja（优先使用 Visual Studio 自带版本）----
    $cmakeCandidates = @()
    $ninjaCandidates = @()
    if ($vsIde) {
        $cmakeCandidates += (Join-Path $vsIde 'CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe')
        $ninjaCandidates += (Join-Path $vsIde 'CommonExtensions\Microsoft\CMake\Ninja\ninja.exe')
    }
    $tc.CMake = Find-Executable $cmakeCandidates 'cmake'
    $tc.Ninja = Find-Executable $ninjaCandidates 'ninja'

    # ---- 系统工具 ----
    $tc.Tar  = Find-Executable @("$env:SystemRoot\System32\tar.exe") 'tar'
    $tc.Curl = Find-Executable @("$env:SystemRoot\System32\curl.exe") 'curl'
    $tc.SevenZip = Find-Executable @(
        'C:\Program Files\7-Zip\7z.exe'
        'C:\Program Files (x86)\7-Zip\7z.exe'
    ) '7z'

    # ---- Qt ----
    $qtCandidates = @()
    if ($env:QT_DIR) { $qtCandidates += $env:QT_DIR }
    try {
        $cfg = Get-DepConfig
        if ($cfg.Qt -and $cfg.Qt.Preferred) { $qtCandidates += $cfg.Qt.Preferred }
    } catch { }
    $qtCandidates += @(
        'D:\Qt\6.9.3\msvc2022_64'
        'C:\Qt\6.9.3\msvc2022_64'
    )
    foreach ($base in @('D:\Qt', 'C:\Qt')) {
        if (Test-Path -LiteralPath $base) {
            foreach ($ver in (Get-ChildItem -LiteralPath $base -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending)) {
                foreach ($kit in (Get-ChildItem -LiteralPath $ver.FullName -Directory -ErrorAction SilentlyContinue |
                                  Where-Object { $_.Name -like 'msvc*' -or $_.Name -like 'win64_msvc*' })) {
                    $qtCandidates += $kit.FullName
                }
            }
        }
    }
    $tc.Qt = Find-ExistingPath $qtCandidates

    $script:Toolchain = $tc
    return $script:Toolchain
}

function Enter-MsvcEnvironment {
    <#
    .SYNOPSIS
        在当前进程中导入 Visual Studio 的 x64 开发环境（等价于调用 VsDevCmd.bat）。
    #>
    param([string]$VsDevCmd)
    if (-not $VsDevCmd) { $VsDevCmd = (Get-Toolchain).VisualStudio }
    if (-not $VsDevCmd) { throw '未找到 Visual Studio（VsDevCmd.bat），无法初始化 MSVC 环境。' }

    Write-Info "初始化 MSVC x64 环境: $VsDevCmd"
    $lines = & cmd.exe /c "`"$VsDevCmd`" -arch=amd64 -host_arch=amd64 -no_logo >nul 2>&1 && set"
    if ($LASTEXITCODE -ne 0 -or -not $lines) { throw "VS 开发环境初始化失败: $VsDevCmd" }
    foreach ($line in $lines) {
        if ($line -match '^(?<k>[^=]+)=(?<v>.*)$') {
            [Environment]::SetEnvironmentVariable($matches['k'], $matches['v'], 'Process')
        }
    }
    # 强制英文诊断信息：Ninja 通过解析 cl.exe 的 /showIncludes 输出来收集头文件依赖，
    # 中文本地化的前缀会导致解析失败并把包含文件列表刷到控制台。
    [Environment]::SetEnvironmentVariable('VSLANG', '1033', 'Process')
    $env:VSLANG = '1033'

    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) { throw 'MSVC 环境初始化后仍未找到 cl.exe。' }
    $cl = (Get-Command cl.exe).Source
    Write-Ok "MSVC 编译器: $cl"
}

function Invoke-Native {
    <#
    .SYNOPSIS
        执行外部命令，失败（退出码非 0）时抛出异常。
    #>
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [string[]]$Arguments = @(),
        [string]$WorkingDirectory = $script:ProjectRoot
    )
    Write-Info "执行: $FilePath $($Arguments -join ' ')"
    Add-BuildLog "RUN   $FilePath $($Arguments -join ' ')"
    $pushed = $false
    if ($WorkingDirectory -and (Test-Path -LiteralPath $WorkingDirectory)) {
        Push-Location -LiteralPath $WorkingDirectory
        $pushed = $true
    }
    try {
        # 过滤 MSVC 的 /showIncludes 列表（本地化前缀下 Ninja 无法解析，会大量刷屏）
        & $FilePath @Arguments 2>&1 | ForEach-Object {
            $line = if ($_ -is [System.Management.Automation.ErrorRecord]) { $_.ToString() } else { [string]$_ }
            if ($line -match '^\s*(注意: 包含文件|Note: including file)') { return }
            Write-Host $line
            Add-Content -LiteralPath (Join-Path $script:LogDir 'build.log') -Value $line -Encoding UTF8 -ErrorAction SilentlyContinue
        }
        $code = $LASTEXITCODE
    } finally {
        if ($pushed) { Pop-Location }
    }
    if ($code -ne 0) {
        throw "命令执行失败（退出码 $code）: $FilePath $($Arguments -join ' ')"
    }
}

# ------------------------------------------------------------ 下载 / 解压 --
function Read-LockFile {
    <#
    .SYNOPSIS
        读取下载锁文件（记录每个依赖实际使用的下载地址与 SHA256）。
    #>
    if (-not (Test-Path -LiteralPath $script:LockFile)) { return @{} }
    try {
        $json = Get-Content -LiteralPath $script:LockFile -Raw -Encoding UTF8 | ConvertFrom-Json
        $hash = @{}
        foreach ($p in $json.PSObject.Properties) { $hash[$p.Name] = $p.Value }
        return $hash
    } catch {
        Write-Warn2 "锁文件损坏，将重新生成: $script:LockFile"
        return @{}
    }
}

function Save-LockFile {
    <#
    .SYNOPSIS
        写回下载锁文件。
    #>
    param([hashtable]$Lock)
    $Lock | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $script:LockFile -Encoding UTF8
}

function Invoke-Download {
    <#
    .SYNOPSIS
        下载文件到本地（支持断点续传与 SHA256 校验，已存在且校验通过时跳过）。
    #>
    param(
        [Parameter(Mandatory)][string]$Url,
        [Parameter(Mandatory)][string]$Destination,
        [string]$Sha256,
        [switch]$Redownload
    )

    $name = Split-Path $Destination -Leaf
    if ($Redownload -and (Test-Path -LiteralPath $Destination)) { Remove-Item -LiteralPath $Destination -Force }

    if (Test-Path -LiteralPath $Destination) {
        $len = (Get-Item -LiteralPath $Destination).Length
        if ($len -gt 0) {
            if ($Sha256) {
                $actual = (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash
                if ($actual -eq $Sha256.ToUpper()) {
                    Write-Ok "缓存校验通过: $name"
                    return $Destination
                }
                Write-Warn2 "缓存校验不通过，重新下载: $name"
            } else {
                Write-Ok "已缓存，跳过下载: $name（$([math]::Round($len / 1MB, 1)) MB）"
                return $Destination
            }
        }
    }

    Write-Info "开始下载: $name <- $Url"
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $curl = (Get-Toolchain).Curl
    $ok = $false
    if ($curl) {
        # --silent --show-error：避免进度表刷屏，下载进度以开始/结束日志体现
        & $curl --location --fail --silent --show-error --retry 3 --retry-delay 3 --continue-at - --output $Destination $Url
        $ok = ($LASTEXITCODE -eq 0)
        if (-not $ok) { Write-Warn2 "curl 下载失败（退出码 $LASTEXITCODE），改用 HTTP 客户端重试" }
    }
    if (-not $ok) {
        $old = $ProgressPreference
        $ProgressPreference = 'SilentlyContinue'
        try {
            Invoke-WebRequest -Uri $Url -OutFile $Destination -UseBasicParsing
            $ok = $true
        } finally {
            $ProgressPreference = $old
        }
    }
    if (-not $ok) { throw "下载失败: $Url" }

    $sw.Stop()
    $size = (Get-Item -LiteralPath $Destination).Length
    if ($size -le 0) { throw "下载结果为空文件: $Destination" }

    if ($Sha256) {
        $actual = (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash
        if ($actual -ne $Sha256.ToUpper()) {
            throw "SHA256 校验失败: $name`n  期望: $($Sha256.ToUpper())`n  实际: $actual"
        }
    }
    Write-Ok ("下载完成: {0}（{1:N1} MB，{2:N1} 秒）" -f $name, ($size / 1MB), $sw.Elapsed.TotalSeconds)
    return $Destination
}

function Expand-DependencyArchive {
    <#
    .SYNOPSIS
        解压依赖压缩包（zip 用 Expand-Archive，7z/tar.gz/tar.xz 等用 bsdtar），已解压时跳过。
    .PARAMETER Probe
        期望在解压结果中出现的相对路径（用于容忍 bsdtar 对个 7z 条目的告警，
        只要所需内容存在即视为解压成功）。
    #>
    param(
        [Parameter(Mandatory)][string]$Archive,
        [Parameter(Mandatory)][string]$Destination,
        [string]$Probe,
        [switch]$Redownload
    )
    if ($Redownload -and (Test-Path -LiteralPath $Destination)) { Remove-Item -LiteralPath $Destination -Recurse -Force }
    if (-not (Test-Path -LiteralPath $Destination)) { New-Item -ItemType Directory -Force -Path $Destination | Out-Null }

    $marker = Join-Path $Destination '.extracted'
    $leaf = Split-Path $Archive -Leaf
    if (Test-Path -LiteralPath $marker) { Write-Ok "已解压，跳过: $leaf"; return $Destination }

    Write-Info "解压 $leaf -> $Destination"
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $exitOk = $true
    if ($Archive -match '\.zip$') {
        Expand-Archive -LiteralPath $Archive -DestinationPath $Destination -Force
    } else {
        $tar = (Get-Toolchain).Tar
        if (-not $tar) { throw "未找到 tar.exe，无法解压 $Archive" }
        & $tar -xf $Archive -C $Destination
        if ($LASTEXITCODE -ne 0) { $exitOk = $false }
    }
    if (-not $exitOk) {
        # bsdtar 读取部分 7z 归档时会因个别条目（名称/属性）非零退出，
        # 这里以“所需内容是否就位”为准，避免因无关条目中断整个流程。
        $probeOk = $false
        if ($Probe) {
            try { [void](Find-ChildDirectory -Root $Destination -Probe $Probe); $probeOk = $true } catch { }
        }
        if ($probeOk) {
            Write-Warn2 "tar 报告了无法识别的归档条目（$leaf），但所需内容已就绪，继续执行。"
        } else {
            throw "解压失败: $Archive"
        }
    }
    $sw.Stop()
    New-Item -ItemType File -Force -Path $marker | Out-Null
    Write-Ok ("解压完成: {0}（{1:N1} 秒）" -f $leaf, $sw.Elapsed.TotalSeconds)
}

# ------------------------------------------------------------ 文件整理 --
function New-CleanDirectory {
    <#
    .SYNOPSIS
        创建（或清空）目录。
    #>
    param([Parameter(Mandatory)][string]$Path)
    if (Test-Path -LiteralPath $Path) { Remove-Item -LiteralPath $Path -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

function Copy-TreeContents {
    <#
    .SYNOPSIS
        复制目录内容（含隐藏文件）到目标目录，必要时先清空目标。
    #>
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$Destination,
        [switch]$Clean
    )
    if (-not (Test-Path -LiteralPath $Source)) { throw "源目录不存在: $Source" }
    if ($Clean -and (Test-Path -LiteralPath $Destination)) { Remove-Item -LiteralPath $Destination -Recurse -Force }
    if (-not (Test-Path -LiteralPath $Destination)) { New-Item -ItemType Directory -Force -Path $Destination | Out-Null }
    foreach ($item in (Get-ChildItem -LiteralPath $Source -Force)) {
        Copy-Item -LiteralPath $item.FullName -Destination $Destination -Recurse -Force
    }
}

function Copy-FileToDirectory {
    <#
    .SYNOPSIS
        复制单个文件到目标目录（目标目录自动创建）。
    #>
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$DestinationDirectory
    )
    if (-not (Test-Path -LiteralPath $Source)) { throw "源文件不存在: $Source" }
    if (-not (Test-Path -LiteralPath $DestinationDirectory)) { New-Item -ItemType Directory -Force -Path $DestinationDirectory | Out-Null }
    Copy-Item -LiteralPath $Source -Destination $DestinationDirectory -Force
}

function Find-ChildDirectory {
    <#
    .SYNOPSIS
        在解压根目录下定位包含指定相对路径的目录（自动兼容压缩包是否带顶层目录）。
    #>
    param(
        [Parameter(Mandatory)][string]$Root,
        [Parameter(Mandatory)][string]$Probe
    )
    $direct = Join-Path $Root $Probe
    if (Test-Path -LiteralPath $direct) { return $Root }
    foreach ($level1 in (Get-ChildItem -LiteralPath $Root -Directory -Force -ErrorAction SilentlyContinue)) {
        if (Test-Path -LiteralPath (Join-Path $level1.FullName $Probe)) { return $level1.FullName }
        foreach ($level2 in (Get-ChildItem -LiteralPath $level1.FullName -Directory -Force -ErrorAction SilentlyContinue)) {
            if (Test-Path -LiteralPath (Join-Path $level2.FullName $Probe)) { return $level2.FullName }
        }
    }
    throw "在 $Root 下未找到包含 '$Probe' 的目录"
}

function Get-StageRoot {
    <#
    .SYNOPSIS
        返回发布暂存目录 work\stage\dependency。
    #>
    Join-Path $script:StageDir 'dependency'
}

function Get-DirectoryManifest {
    <#
    .SYNOPSIS
        生成目录清单（相对路径、大小、可选 SHA256 与其“行尾归一化”SHA256）。
    .PARAMETER WithHash
        计算 SHA256；其中 Sha256Lf 为把 CRLF 统一成 LF 之后计算的哈希，
        用于识别“仅行尾不同、内容等价”的文件（Windows 上经 Git/编辑器处理过的文本文件常见）。
    #>
    param(
        [Parameter(Mandatory)][string]$Root,
        [switch]$WithHash
    )
    if (-not (Test-Path -LiteralPath $Root)) { throw "目录不存在: $Root" }
    $resolved = (Resolve-Path -LiteralPath $Root).Path.TrimEnd('\')
    Get-ChildItem -LiteralPath $resolved -Recurse -File -Force | ForEach-Object {
        $rel = $_.FullName.Substring($resolved.Length).TrimStart('\')
        $hash = ''
        $hashLf = ''
        if ($WithHash) {
            $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
            $hashLf = Get-NormalizedSha256 -Path $_.FullName
        }
        [pscustomobject]@{ Path = $rel; Length = $_.Length; Sha256 = $hash; Sha256Lf = $hashLf }
    }
}

function Get-NormalizedSha256 {
    <#
    .SYNOPSIS
        计算内容在“CRLF -> LF”归一化之后的 SHA256。
    .NOTES
        使用 ISO-8859-1 做字节 <-> 字符的 1:1 映射，保证除行尾外不改变任何字节。
    #>
    param([Parameter(Mandatory)][string]$Path)
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $enc = [System.Text.Encoding]::GetEncoding(28591)
    $text = $enc.GetString($bytes).Replace("`r`n", "`n")
    $normalized = $enc.GetBytes($text)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return (($sha.ComputeHash($normalized) | ForEach-Object { $_.ToString('x2') }) -join '')
    } finally {
        $sha.Dispose()
    }
}

function Get-ArchivePath {
    <#
    .SYNOPSIS
        按配置项中的文件名返回下载缓存中的完整路径。
    #>
    param([Parameter(Mandatory)][string]$FileName)
    Join-Path $script:DownloadDir $FileName
}
