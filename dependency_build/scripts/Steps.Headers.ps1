<#
.SYNOPSIS
    纯头文件依赖（Boost / Eigen / magic_enum / CLI11）整理步骤。
#>

function Invoke-StepHeaders {
    <#
    .SYNOPSIS
        解压并整理 Boost、Eigen、magic_enum、CLI11 到暂存目录。
    #>
    Write-Head '步骤：头文件类依赖（Boost / Eigen / magic_enum / CLI11）'
    $stage = Get-StageRoot

    # ------------------------------------------------------------ Boost --
    Write-Info '整理 Boost 1.90.0'
    $extract = Join-Path $script:SourceDir 'boost'
    Expand-DependencyArchive -Archive (Get-LockedArchive 'boost') -Destination $extract -Probe 'boost\version.hpp' -Redownload:$Force
    $root = Find-ChildDirectory -Root $extract -Probe 'boost\version.hpp'
    $dst = Join-Path $stage 'include\boost_1_90_0'
    New-CleanDirectory $dst
    Copy-TreeContents -Source (Join-Path $root 'boost') -Destination (Join-Path $dst 'boost')
    Copy-FileToDirectory -Source (Join-Path $root 'LICENSE_1_0.txt') -DestinationDirectory $dst
    Write-Ok 'Boost -> include\boost_1_90_0'

    # ------------------------------------------------------------ Eigen --
    Write-Info '整理 Eigen 5.0.0'
    $extract = Join-Path $script:SourceDir 'eigen'
    Expand-DependencyArchive -Archive (Get-LockedArchive 'eigen') -Destination $extract -Probe 'Eigen\Dense' -Redownload:$Force
    $root = Find-ChildDirectory -Root $extract -Probe 'Eigen\Dense'
    $dst = Join-Path $stage 'include\eigen_5_0_0'
    New-CleanDirectory $dst
    foreach ($sub in @('Eigen', 'unsupported')) {
        Copy-TreeContents -Source (Join-Path $root $sub) -Destination (Join-Path $dst $sub)
    }
    foreach ($lic in (Get-ChildItem -LiteralPath $root -File -Force | Where-Object { $_.Name -like 'COPYING*' -or $_.Name -eq 'LICENSE' })) {
        Copy-FileToDirectory -Source $lic.FullName -DestinationDirectory $dst
    }
    Write-Ok 'Eigen -> include\eigen_5_0_0'

    # ------------------------------------------------------- magic_enum --
    Write-Info '整理 magic_enum 0.9.8'
    $extract = Join-Path $script:SourceDir 'magic_enum'
    Expand-DependencyArchive -Archive (Get-LockedArchive 'magic_enum') -Destination $extract -Probe 'include\magic_enum' -Redownload:$Force
    $root = Find-ChildDirectory -Root $extract -Probe 'include\magic_enum'
    $dst = Join-Path $stage 'include\magic_enum_0.9.8'
    New-CleanDirectory $dst
    Copy-TreeContents -Source (Join-Path $root 'include\magic_enum') -Destination (Join-Path $dst 'magic_enum')
    Copy-FileToDirectory -Source (Join-Path $root 'LICENSE') -DestinationDirectory $dst
    Write-Ok 'magic_enum -> include\magic_enum_0.9.8'

    # ----------------------------------------------------------- CLI11 --
    Write-Info '整理 CLI11 2.6.2'
    $dst = Join-Path $stage 'include\CLI11'
    New-CleanDirectory $dst
    Copy-Item -LiteralPath (Get-LockedArchive 'cli11') -Destination (Join-Path $dst 'CLI11.hpp') -Force
    Write-Ok 'CLI11 -> include\CLI11\CLI11.hpp'

    Write-Ok '头文件类依赖整理完成'
}
