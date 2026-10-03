<#
.SYNOPSIS
    工具链检测与依赖包下载步骤。
#>

function Invoke-StepCheckToolchain {
    <#
    .SYNOPSIS
        检测重建 dependency 所需的工具链，并输出可用性报告。
    #>
    Write-Head '步骤：工具链检测'
    $tc = Get-Toolchain

    $rows = @(
        @{ Name = 'Visual Studio (VsDevCmd)'; Path = $tc.VisualStudio; Required = $true }
        @{ Name = 'CMake';                    Path = $tc.CMake;      Required = $true }
        @{ Name = 'Ninja';                    Path = $tc.Ninja;      Required = $true }
        @{ Name = 'tar (bsdtar)';             Path = $tc.Tar;        Required = $true }
        @{ Name = 'curl';                     Path = $tc.Curl;       Required = $false }
        @{ Name = '7-Zip（可选）';             Path = $tc.SevenZip;   Required = $false }
        @{ Name = 'Qt 6.9.3 msvc2022_64';     Path = $tc.Qt;         Required = $false }
    )

    $missing = @()
    foreach ($r in $rows) {
        if ($r.Path) {
            Write-Host ('  [OK] {0,-24} {1}' -f $r.Name, $r.Path) -ForegroundColor Green
        } elseif ($r.Required) {
            Write-Host ('  [!!] {0,-24} 未找到（必需）' -f $r.Name) -ForegroundColor Red
            $missing += $r.Name
        } else {
            Write-Host ('  [--] {0,-24} 未找到（相关步骤会跳过或需要手动指定）' -f $r.Name) -ForegroundColor Yellow
        }
    }

    if ($missing.Count -gt 0) { throw "缺少必需工具: $($missing -join ', ')" }
    if (-not $tc.Qt) { Write-Warn2 '未检测到 Qt 6.9.3（msvc2022_64），QXlsx 步骤将无法完成；可通过 QT_DIR 环境变量或 config\dependencies.psd1 指定。' }
    Write-Ok '工具链检测通过'
}

function Get-FetchTargets {
    <#
    .SYNOPSIS
        返回需要下载的依赖包列表（键名 -> 候选下载源）。
    #>
    $cfg = Get-DepConfig
    @(
        @{ Key = 'boost';           Candidates = $cfg.Boost.Archives }
        @{ Key = 'eigen';           Candidates = $cfg.Eigen.Archives }
        @{ Key = 'magic_enum';      Candidates = $cfg.MagicEnum.Archives }
        @{ Key = 'cli11';           Candidates = $cfg.Cli11.Archives }
        @{ Key = 'ffmpeg_prebuilt'; Candidates = $cfg.FFmpeg.Prebuilt }
        @{ Key = 'ffmpeg_source';   Candidates = $cfg.FFmpeg.Source }
        @{ Key = 'assimp';          Candidates = $cfg.Assimp.Archives }
        @{ Key = 'qxlsx';           Candidates = $cfg.QXlsx.Archives }
    )
}

function Invoke-StepFetch {
    <#
    .SYNOPSIS
        下载全部依赖包到 work\downloads，并记录实际使用的下载源与 SHA256。
    #>
    Write-Head '步骤：下载依赖源码包 / 预编译包'

    $lock = Read-LockFile
    if ($lock.Count -gt 0 -and $Force) {
        Write-Info '已指定 -Force，忽略下载锁与缓存'
        $lock = @{}
    }

    foreach ($target in (Get-FetchTargets)) {
        $entry = $null

        # 1) 命中锁文件且缓存文件校验通过 -> 直接复用
        if (-not $Force -and $lock.ContainsKey($target.Key)) {
            $locked = $lock[$target.Key]
            $cached = Get-ArchivePath $locked.File
            if (Test-Path -LiteralPath $cached) {
                $hashOk = $true
                if ($locked.Sha256) {
                    $actual = (Get-FileHash -LiteralPath $cached -Algorithm SHA256).Hash
                    $hashOk = ($actual -eq $locked.Sha256.ToUpper())
                }
                if ($hashOk) {
                    $entry = $locked
                    Write-Ok "$($target.Key): 复用缓存 $($locked.File)（SHA256 校验通过）"
                } else {
                    Write-Warn2 "$($target.Key): 缓存文件校验失败，重新下载"
                }
            }
        }

        # 2) 依次尝试候选下载源
        if (-not $entry) {
            $errors = @()
            foreach ($candidate in $target.Candidates) {
                try {
                    $dest = Get-ArchivePath $candidate.File
                    Invoke-Download -Url $candidate.Url -Destination $dest -Redownload:$Force | Out-Null
                    $hash = (Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash
                    $entry = @{ File = $candidate.File; Url = $candidate.Url; Sha256 = $hash }
                    break
                } catch {
                    $errors += "$($candidate.Url)`n      -> $($_.Exception.Message)"
                }
            }
            if (-not $entry) {
                throw "依赖 '$($target.Key)' 的所有下载源均失败:`n    $($errors -join "`n    ")"
            }
            $sizeMb = [math]::Round((Get-Item -LiteralPath (Get-ArchivePath $entry.File)).Length / 1MB, 1)
            Write-Ok "$($target.Key): $($entry.File)（$sizeMb MB，SHA256 $($entry.Sha256.Substring(0, 12))...）"
        }

        $lock[$target.Key] = $entry
    }

    Save-LockFile $lock
    Write-Ok "全部依赖包已就绪: $script:DownloadDir"
}

function Get-LockedArchive {
    <#
    .SYNOPSIS
        返回指定依赖在下载缓存中的压缩包路径（不存在时抛出提示）。
    #>
    param([Parameter(Mandatory)][string]$Key)

    $lock = Read-LockFile
    if (-not $lock.ContainsKey($Key)) { throw "依赖 '$Key' 尚未下载，请先执行: -Step fetch" }
    $path = Get-ArchivePath $lock[$Key].File
    if (-not (Test-Path -LiteralPath $path)) { throw "下载缓存缺失: $path，请先执行: -Step fetch" }
    return $path
}
