<#
.SYNOPSIS
    QXlsx(Qt6) 1.5.0 源码编译步骤（Qt 6.9.3 + MSVC x64，Release + Debug）。
#>

function Invoke-StepQXlsx {
    <#
    .SYNOPSIS
        编译 QXlsx(Qt6) 1.5.0 静态库，产出 QXlsxQt6.lib / QXlsxQt6d.lib 及公开头文件。
    .NOTES
        * QXlsx 的 CMake 会把输出名设为 QXlsxQt${QT_VERSION_MAJOR}（即 QXlsxQt6），
          公开头文件安装到 include/QXlsxQt6，与本仓库既有布局一致；
        * Debug 版本通过 CMAKE_DEBUG_POSTFIX=d 得到 QXlsxQt6d.lib；
        * 通过 cmake --install 到 work\install 下的临时前缀，再按需取用，避免手工挑选文件。
    #>
    Write-Head '步骤：编译 QXlsx(Qt6) 1.5.0（Qt 6.9.3 + MSVC x64，Release + Debug）'
    $stage = Get-StageRoot
    $tc = Get-Toolchain
    if (-not $tc.Qt) {
        throw '未找到 Qt 6.9.3（msvc2022_64）。请安装 Qt 或设置 QT_DIR 环境变量后重试。'
    }
    Write-Info "Qt 路径: $($tc.Qt)"

    # -------------------------------------------------------- 源码准备 --
    $extract = Join-Path $script:SourceDir 'qxlsx'
    Expand-DependencyArchive -Archive (Get-LockedArchive 'qxlsx') -Destination $extract -Probe 'QXlsx\CMakeLists.txt' -Redownload:$Force
    $repo = Find-ChildDirectory -Root $extract -Probe 'QXlsx\CMakeLists.txt'
    $src = Join-Path $repo 'QXlsx'
    Write-Info "QXlsx 源码: $src"

    Enter-MsvcEnvironment

    $parallelArgs = @('--parallel')
    if ($Jobs -gt 0) { $parallelArgs += "$Jobs" }

    $installBase = Join-Path $script:WorkRoot 'install'
    if (-not (Test-Path -LiteralPath $installBase)) { New-Item -ItemType Directory -Force -Path $installBase | Out-Null }

    foreach ($cfg in @(@{ Name = 'Release'; Postfix = '' }, @{ Name = 'Debug'; Postfix = 'd' })) {
        $name = $cfg.Name
        $build = Join-Path $script:BuildDir "qxlsx-$($name.ToLower())"
        $prefix = Join-Path $installBase "qxlsx-$($name.ToLower())"
        Write-Info "配置 QXlsx $name -> $build"

        $configureArgs = @(
            '-S', $src
            '-B', $build
            '-G', 'Ninja'
            "-DCMAKE_BUILD_TYPE=$name"
            "-DCMAKE_MAKE_PROGRAM=$($tc.Ninja)"
            "-DCMAKE_PREFIX_PATH=$($tc.Qt)"
            "-DCMAKE_INSTALL_PREFIX=$prefix"
            '-DBUILD_SHARED_LIBS=OFF'
        )
        if ($cfg.Postfix) { $configureArgs += "-DCMAKE_DEBUG_POSTFIX=$($cfg.Postfix)" }
        if ($Force -and (Test-Path -LiteralPath $build)) { Remove-Item -LiteralPath $build -Recurse -Force }

        Invoke-Native -FilePath $tc.CMake -Arguments $configureArgs
        Invoke-Native -FilePath $tc.CMake -Arguments (@('--build', $build, '--target', 'QXlsx') + $parallelArgs)
        Invoke-Native -FilePath $tc.CMake -Arguments @('--install', $build)
        Write-Ok "QXlsx $name 构建并安装完成"
    }

    # ---------------------------------------------------- 产物整理 --
    $incDst = Join-Path $stage 'include\qxlsx_qt6_1.5.0'
    $libDst = Join-Path $stage 'lib\QXlsxQt6'
    New-CleanDirectory $incDst
    New-CleanDirectory $libDst

    # 公开头文件：优先取 Release 安装结果（与 Debug 一致）
    $headerSrc = Join-Path $installBase 'qxlsx-release\include\QXlsxQt6'
    if (-not (Test-Path -LiteralPath $headerSrc)) { $headerSrc = Join-Path $installBase 'qxlsx-debug\include\QXlsxQt6' }
    if (-not (Test-Path -LiteralPath $headerSrc)) { throw '未找到 QXlsx 安装后的头文件目录。' }
    Copy-TreeContents -Source $headerSrc -Destination (Join-Path $incDst 'QXlsxQt6')
    Copy-FileToDirectory -Source (Join-Path $repo 'LICENSE') -DestinationDirectory $incDst
    Write-Ok 'QXlsx 头文件 -> include\qxlsx_qt6_1.5.0'

    foreach ($cfg in @(@{ Name = 'Release'; File = 'QXlsxQt6.lib' }, @{ Name = 'Debug'; File = 'QXlsxQt6d.lib' })) {
        $libSrc = Join-Path $installBase "qxlsx-$($cfg.Name.ToLower())\lib\$($cfg.File)"
        if (-not (Test-Path -LiteralPath $libSrc)) {
            # 兜底：直接在构建目录中查找
            $found = Get-ChildItem -LiteralPath (Join-Path $script:BuildDir "qxlsx-$($cfg.Name.ToLower())") -Recurse -File -Filter $cfg.File |
                     Select-Object -First 1
            if (-not $found) { throw "未找到 QXlsx $($cfg.Name) 产物: $($cfg.File)" }
            $libSrc = $found.FullName
        }
        Copy-Item -LiteralPath $libSrc -Destination (Join-Path $libDst $cfg.File) -Force
    }
    Write-Ok 'QXlsx 静态库 -> lib\QXlsxQt6'

    Write-Ok 'QXlsx 依赖整理完成'
}
