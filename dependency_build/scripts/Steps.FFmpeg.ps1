<#
.SYNOPSIS
    FFmpeg 9.0.2 依赖整理步骤（官方 Windows shared 预编译包 + 源码 presets）。
#>

function Invoke-StepFFmpeg {
    <#
    .SYNOPSIS
        解压 gyan.dev 官方 shared 构建，整理 bin / lib / include / preset 到暂存目录。
    .NOTES
        说明：Windows 下从源码完整编译 FFmpeg 需要 MSYS2 + MinGW 工具链，
        因此这里使用官方发布的 Windows shared 构建（版本串 9.0.2-full_build 与现有目录一致），
        仅从源码包中提取 presets 目录。
    #>
    Write-Head '步骤：FFmpeg 9.0.2（官方 shared 预编译包 + 源码 presets）'
    $stage = Get-StageRoot

    # ------------------------------------------------------ 预编译共享库 --
    Write-Info '解压官方 shared 构建包'
    $extract = Join-Path $script:SourceDir 'ffmpeg-shared'
    Expand-DependencyArchive -Archive (Get-LockedArchive 'ffmpeg_prebuilt') -Destination $extract -Probe 'include\libavcodec' -Redownload:$Force
    $root = Find-ChildDirectory -Root $extract -Probe 'include\libavcodec'

    Copy-TreeContents -Source (Join-Path $root 'bin') -Destination (Join-Path $stage 'bin\ffmpeg') -Clean
    Write-Ok 'FFmpeg 运行时 -> bin\ffmpeg'
    Copy-TreeContents -Source (Join-Path $root 'lib') -Destination (Join-Path $stage 'lib\ffmpeg') -Clean
    Write-Ok 'FFmpeg 导入库 -> lib\ffmpeg'

    $incDst = Join-Path $stage 'include\ffmpeg_9_0_2'
    New-CleanDirectory $incDst
    Copy-TreeContents -Source (Join-Path $root 'include') -Destination $incDst
    foreach ($f in @('LICENSE', 'README.txt')) {
        $p = Join-Path $root $f
        if (Test-Path -LiteralPath $p) { Copy-FileToDirectory -Source $p -DestinationDirectory $incDst }
    }
    Write-Ok 'FFmpeg 头文件 -> include\ffmpeg_9_0_2'

    # -------------------------------------------------------------- presets --
    $presetDst = Join-Path $stage 'preset\ffmpeg_preset'
    $prebuiltPresets = Join-Path $root 'presets'
    if (Test-Path -LiteralPath $prebuiltPresets) {
        New-CleanDirectory $presetDst
        Copy-TreeContents -Source $prebuiltPresets -Destination $presetDst
        Write-Ok 'FFmpeg presets（来自预编译包） -> preset\ffmpeg_preset'
    } else {
        Write-Info '预编译包中不含 presets，改从 FFmpeg 源码包提取'
        $srcExtract = Join-Path $script:SourceDir 'ffmpeg-src'
        Expand-DependencyArchive -Archive (Get-LockedArchive 'ffmpeg_source') -Destination $srcExtract -Probe 'presets' -Redownload:$Force
        $srcRoot = Find-ChildDirectory -Root $srcExtract -Probe 'presets'
        New-CleanDirectory $presetDst
        Copy-TreeContents -Source (Join-Path $srcRoot 'presets') -Destination $presetDst
        Write-Ok 'FFmpeg presets（来自源码包） -> preset\ffmpeg_preset'
    }

    Write-Ok 'FFmpeg 依赖整理完成'
}
