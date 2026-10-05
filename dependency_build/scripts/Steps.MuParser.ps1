<#
.SYNOPSIS
    muparser 2.3.5 源码整理步骤（纯源码依赖，无需编译）。

    .NOTES
        muparser 在 Egret Physics 中由主程序直接编译（CMakeLists.txt 里以
        egret_muparser 静态库目标的形式引用 dependency\src\muparser_2.3.5 下的 7 个核心 .cpp），
        因此本步骤只负责「下载 → 解压 → 整理头文件与源文件到 stage」，不产出 .lib。
        产物布局与现有 dependency 保持一致：
            include\muparser_2.3.5\  ← 公开头文件 + LICENSE + README
            src\muparser_2.3.5\      ← 7 个核心 .cpp（不含 DLL 包装与自测）
#>

function Get-MuParserArchive {
    <#
    .SYNOPSIS
        取（必要时下载）muparser 源码包。
    .NOTES
        不依赖 fetch 步骤与 sources.lock.json：本步骤只下载自己需要的包，
        便于单独重跑，也不会顺带触发其它体积较大的依赖下载。
    #>
    param([Parameter(Mandatory)][string]$Root)

    $configFile = Join-Path $script:ConfigDir 'dependencies.psd1'
    $deps = Import-PowerShellDataFile -LiteralPath $configFile
    $entry = $deps.MuParser
    if (-not $entry) { throw 'dependencies.psd1 中缺少 MuParser 条目。' }

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
    throw 'muparser 源码包全部下载源均失败。'
}

function Invoke-StepMuParser {
    <#
    .SYNOPSIS
        整理 muparser 2.3.5 的公开头文件与核心源文件。
    #>
    Write-Head '步骤：整理 muparser 2.3.5（纯源码依赖）'

    $extract = Join-Path $script:SourceDir 'muparser'
    Expand-DependencyArchive -Archive (Get-MuParserArchive -Root $extract) `
        -Destination $extract -Probe 'include\muParser.h' -Redownload:$Force
    $repo = Find-ChildDirectory -Root $extract -Probe 'include\muParser.h'
    Write-Info "muparser 源码: $repo"

    $stage = Get-StageRoot
    $incDst = Join-Path $stage 'include\muparser_2.3.5'
    $srcDst = Join-Path $stage 'src\muparser_2.3.5'
    New-CleanDirectory $incDst
    New-CleanDirectory $srcDst

    # ---------------------------------------------------- 公开头文件 --
    $includeSrc = Join-Path $repo 'include'
    if (-not (Test-Path -LiteralPath $includeSrc)) { throw "未找到 muparser 头文件目录: $includeSrc" }
    Copy-TreeContents -Source $includeSrc -Destination $incDst
    foreach ($extra in @('LICENSE', 'README.md')) {
        $file = Join-Path $repo $extra
        if (Test-Path -LiteralPath $file) { Copy-FileToDirectory -Source $file -DestinationDirectory $incDst }
    }
    Write-Ok 'muparser 头文件与许可证 -> include\muparser_2.3.5'

    # -------------------------------------------- 核心源文件（编译用） --
    # 只取标准解析器所需的核心实现；muParserDLL.cpp 是 C 接口包装、
    # muParserTest.cpp 是自测，二者不参与主程序编译。
    $coreSources = @(
        'muParser.cpp', 'muParserBase.cpp', 'muParserBytecode.cpp', 'muParserCallback.cpp',
        'muParserError.cpp', 'muParserInt.cpp', 'muParserTokenReader.cpp'
    )
    foreach ($name in $coreSources) {
        $file = Join-Path (Join-Path $repo 'src') $name
        if (-not (Test-Path -LiteralPath $file)) { throw "缺少 muparser 源文件: $file" }
        Copy-FileToDirectory -Source $file -DestinationDirectory $srcDst
    }
    Write-Ok "muparser 核心源文件（$($coreSources.Count) 个）-> src\muparser_2.3.5"

    Write-Ok 'muparser 依赖整理完成'
}
