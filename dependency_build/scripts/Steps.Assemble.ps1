<#
.SYNOPSIS
    把 work\stage 中整理好的依赖发布为项目根目录下的 dependency。
#>

function Invoke-StepAssemble {
    <#
    .SYNOPSIS
        校验暂存目录完整性，并将其发布到 dependency 目录（发布前自动备份旧目录）。
    #>
    Write-Head '步骤：汇总发布 dependency 目录'

    $stage = Get-StageRoot
    if (-not (Test-Path -LiteralPath $stage)) { throw "暂存目录不存在: $stage，请先执行构建步骤" }

    # ------------------------------------------------------ 完整性检查 --
    $required = @(
        'include\CLI11\CLI11.hpp',
        'include\assimp_6_0_2\assimp\scene.h',
        'include\boost_1_90_0\boost\version.hpp',
        'include\eigen_5_0_0\Eigen\Dense',
        'include\ffmpeg_9_0_2\libavcodec\avcodec.h',
        'include\magic_enum_0.9.8\magic_enum\magic_enum.hpp',
        'include\qxlsx_qt6_1.5.0\QXlsxQt6\xlsxdocument.h',
        'lib\Assimp\assimp-vc143-mt.lib',
        'lib\Assimp\assimp-vc143-mtd.lib',
        'lib\QXlsxQt6\QXlsxQt6.lib',
        'lib\QXlsxQt6\QXlsxQt6d.lib',
        'lib\ffmpeg\avcodec.lib',
        'bin\Assimp\assimp-vc143-mt.dll',
        'bin\Assimp\assimp-vc143-mtd.dll',
        'bin\ffmpeg\avcodec-63.dll',
        'preset\ffmpeg_preset'
    )
    $missing = @()
    foreach ($r in $required) {
        if (-not (Test-Path -LiteralPath (Join-Path $stage $r))) { $missing += $r }
    }
    if ($missing.Count -gt 0) {
        throw "暂存目录缺少必需内容，已中止发布:`n    $($missing -join "`n    ")"
    }
    Write-Ok "暂存目录完整性检查通过（$($required.Count) 项关键路径）"

    # ---------------------------------------------------------- 发布 --
    $target = $script:OutputDir
    $targetParent = Split-Path -Parent $target
    if (-not (Test-Path -LiteralPath $targetParent)) { New-Item -ItemType Directory -Force -Path $targetParent | Out-Null }

    if (Test-Path -LiteralPath $target) {
        $backupRoot = Join-Path $script:WorkRoot 'backup'
        if (-not (Test-Path -LiteralPath $backupRoot)) { New-Item -ItemType Directory -Force -Path $backupRoot | Out-Null }
        $backup = Join-Path $backupRoot ("dependency_" + (Get-Date -Format 'yyyyMMdd_HHmmss'))
        Write-Info "备份现有目录: $target -> $backup"
        Move-Item -LiteralPath $target -Destination $backup -Force
        Write-Ok "旧目录已备份（回滚命令：Move-Item -LiteralPath '$backup' -Destination '$target' -Force）"
    }

    Write-Info "发布: $stage -> $target"
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    foreach ($item in (Get-ChildItem -LiteralPath $stage -Force)) {
        Copy-Item -LiteralPath $item.FullName -Destination $target -Recurse -Force
    }

    # ---------------------------------------------------------- 统计 --
    $files = Get-ChildItem -LiteralPath $target -Recurse -File -Force
    $totalMb = [math]::Round((($files | Measure-Object -Property Length -Sum).Sum) / 1MB, 1)
    Write-Ok "发布完成: $($files.Count) 个文件，共 $totalMb MB"
    Write-Host ''
    Write-Host '  顶层结构:' -ForegroundColor DarkGray
    foreach ($d in (Get-ChildItem -LiteralPath $target -Directory -Force | Sort-Object Name)) {
        $count = (Get-ChildItem -LiteralPath $d.FullName -Recurse -File -Force).Count
        Write-Host ("    {0,-10} {1,6} 个文件" -f $d.Name, $count) -ForegroundColor DarkGray
    }
}
