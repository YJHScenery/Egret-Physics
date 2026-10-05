<#
.SYNOPSIS
    QCustomPlot 2.1.1 源码整理步骤（纯源码依赖，无需编译）。

    .NOTES
        QCustomPlot 是单头单源库，在 Egret Physics 中由主程序直接编译
        （CMakeLists.txt 里引用 dependency\src\qcustomplot_2.1.1\qcustomplot.cpp），
        因此本步骤只负责「下载 → 解压 → 整理到 stage」，不产出 .lib。
        产物布局与现有 dependency 保持一致：
            include\qcustomplot_2.1.1\  ← qcustomplot.h + GPL.txt
            src\qcustomplot_2.1.1\      ← qcustomplot.cpp
        许可证为 GPLv3，GPL.txt 随源码一并发布并已在项目根 NOTICE 中登记。
#>

function Get-QCustomPlotArchive {
    <#
    .SYNOPSIS
        取（必要时下载）QCustomPlot 源码包。
    .NOTES
        官方发布地址为 qcustomplot.com（非 GitHub），包内目录名为 qcustomplot-source。
    #>
    param([Parameter(Mandatory)][string]$Root)

    $configFile = Join-Path $script:ConfigDir 'dependencies.psd1'
    $deps = Import-PowerShellDataFile -LiteralPath $configFile
    $entry = $deps.QCustomPlot
    if (-not $entry) { throw 'dependencies.psd1 中缺少 QCustomPlot 条目。' }

    foreach ($archive in $entry.Archives) {
        $path = Join-Path $script:DownloadDir $archive.File
        if ((Test-Path -LiteralPath $path) -and -not $Force) {
            Write-Ok "已缓存: $($archive.File)"
            return $path
        }
        try {
            # 用 $null = 吃掉 Invoke-Download 的输出，保证本函数只返回一个路径字符串
            $null = Invoke-Download -Url $archive.Url -Destination $path -Redownload:$Force
            return $path
        }
        catch {
            Write-Warn2 "下载失败，尝试下一个源: $($archive.Url)"
        }
    }
    throw 'QCustomPlot 源码包全部下载源均失败。'
}

function Invoke-StepQCustomPlot {
    <#
    .SYNOPSIS
        整理 QCustomPlot 2.1.1 的头文件、源文件与 GPL 许可证。
    #>
    Write-Head '步骤：整理 QCustomPlot 2.1.1（纯源码依赖）'

    $extract = Join-Path $script:SourceDir 'qcustomplot'
    Expand-DependencyArchive -Archive (Get-QCustomPlotArchive -Root $extract) `
        -Destination $extract -Probe 'qcustomplot-source\qcustomplot.h' -Redownload:$Force
    $repo = Find-ChildDirectory -Root $extract -Probe 'qcustomplot.h'
    Write-Info "QCustomPlot 源码: $repo"

    $stage = Get-StageRoot
    $incDst = Join-Path $stage 'include\qcustomplot_2.1.1'
    $srcDst = Join-Path $stage 'src\qcustomplot_2.1.1'
    New-CleanDirectory $incDst
    New-CleanDirectory $srcDst

    # ------------------------------------------------- 头文件与许可证 --
    Copy-FileToDirectory -Source (Join-Path $repo 'qcustomplot.h') -DestinationDirectory $incDst
    $license = Join-Path $repo 'GPL.txt'
    if (-not (Test-Path -LiteralPath $license)) { throw "缺少 QCustomPlot 许可证文件: $license" }
    Copy-FileToDirectory -Source $license -DestinationDirectory $incDst
    Write-Ok 'QCustomPlot 头文件与 GPL.txt -> include\qcustomplot_2.1.1'

    # ------------------------------------------------------------ 源文件 --
    Copy-FileToDirectory -Source (Join-Path $repo 'qcustomplot.cpp') -DestinationDirectory $srcDst
    Write-Ok 'QCustomPlot 源文件 -> src\qcustomplot_2.1.1'

    Write-Ok 'QCustomPlot 依赖整理完成'
}
