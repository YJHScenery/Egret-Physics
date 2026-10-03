<#
.SYNOPSIS
    Assimp 6.0.2 源码编译步骤（MSVC x64，Release + Debug）。
#>

function Invoke-StepAssimp {
    <#
    .SYNOPSIS
        使用 MSVC x64 编译 Assimp 6.0.2 动态库，产出与现有目录一致的
        assimp-vc143-mt(d).dll / assimp-vc143-mt(d).lib 及头文件。
    .NOTES
        * 使用 CMake + Ninja，Release 与 Debug 各建一个构建目录；
        * MSVC 工具集版本不同（例如 VS 2026 的 v145）时 Assimp 生成的库名会不同，
          这里统一按工程既有命名（vc143-mt / vc143-mtd）整理，导入库与 DLL 名称保持一致即可正常链接与加载。
    #>
    Write-Head '步骤：编译 Assimp 6.0.2（MSVC x64，Release + Debug）'
    $stage = Get-StageRoot
    $tc = Get-Toolchain

    # -------------------------------------------------------- 源码准备 --
    $extract = Join-Path $script:SourceDir 'assimp'
    Expand-DependencyArchive -Archive (Get-LockedArchive 'assimp') -Destination $extract -Probe 'include\assimp\scene.h' -Redownload:$Force
    $src = Find-ChildDirectory -Root $extract -Probe 'include\assimp\scene.h'
    Write-Info "Assimp 源码: $src"

    Enter-MsvcEnvironment

    $parallelArgs = @('--parallel')
    if ($Jobs -gt 0) { $parallelArgs += "$Jobs" }

    $outputs = @{}
    foreach ($cfg in @(@{ Name = 'Release'; Suffix = 'mt' }, @{ Name = 'Debug'; Suffix = 'mtd' })) {
        $name = $cfg.Name
        $build = Join-Path $script:BuildDir "assimp-$($name.ToLower())"
        Write-Info "配置 Assimp $name -> $build"

        $configureArgs = @(
            '-S', $src
            '-B', $build
            '-G', 'Ninja'
            "-DCMAKE_BUILD_TYPE=$name"
            "-DCMAKE_MAKE_PROGRAM=$($tc.Ninja)"
            '-DBUILD_SHARED_LIBS=ON'
            '-DASSIMP_BUILD_TESTS=OFF'
            '-DASSIMP_BUILD_ASSIMP_TOOLS=OFF'
            '-DASSIMP_BUILD_SAMPLES=OFF'
            '-DASSIMP_INSTALL=OFF'
            '-DASSIMP_WARNINGS_AS_ERRORS=OFF'
            '-DASSIMP_BUILD_ZLIB=ON'
            # 忽略宿主仓库的 Git 信息：否则 Assimp 会跨目录找到本工程的 .git，
            # 把本仓库的分支名与提交号写进生成的 revision.h，破坏产物可复现性。
            '-DASSIMP_IGNORE_GIT_HASH=ON'
            # 强制使用工程既有的库名前缀，避免因本机工具集版本（如 VS 2026 的 v145）产生不同命名
            '-DLIBRARY_SUFFIX=-vc143-mt'
        )
        if ($Force -and (Test-Path -LiteralPath $build)) { Remove-Item -LiteralPath $build -Recurse -Force }
        Invoke-Native -FilePath $tc.CMake -Arguments $configureArgs
        Invoke-Native -FilePath $tc.CMake -Arguments (@('--build', $build, '--target', 'assimp') + $parallelArgs)

        # 定位产物（Assimp 默认把运行时输出放在 bin，导入库放在 lib）
        $dll = Get-ChildItem -LiteralPath $build -Recurse -File -Filter 'assimp-*.dll' |
               Sort-Object { $_.DirectoryName -notmatch '\\bin$' } | Select-Object -First 1
        $imp = Get-ChildItem -LiteralPath $build -Recurse -File -Filter 'assimp-*.lib' |
               Sort-Object { $_.DirectoryName -notmatch '\\lib$' } | Select-Object -First 1
        if (-not $dll -or -not $imp) { throw "未找到 Assimp $name 构建产物（dll/lib），请检查构建日志。" }

        $outputs[$name] = @{ Dll = $dll.FullName; Lib = $imp.FullName }
        Write-Ok "Assimp $name 构建完成: $(Split-Path $dll.FullName -Leaf) / $(Split-Path $imp.FullName -Leaf)"
    }

    # ---------------------------------------------------- 产物整理 --
    $binDst = Join-Path $stage 'bin\Assimp'
    $libDst = Join-Path $stage 'lib\Assimp'
    New-CleanDirectory $binDst
    New-CleanDirectory $libDst
    foreach ($cfg in @(@{ Name = 'Release'; Suffix = 'mt' }, @{ Name = 'Debug'; Suffix = 'mtd' })) {
        $o = $outputs[$cfg.Name]
        Copy-Item -LiteralPath $o.Dll -Destination (Join-Path $binDst "assimp-vc143-$($cfg.Suffix).dll") -Force
        Copy-Item -LiteralPath $o.Lib -Destination (Join-Path $libDst "assimp-vc143-$($cfg.Suffix).lib") -Force
    }
    Write-Ok 'Assimp 二进制 -> bin\Assimp、lib\Assimp'

    # ---------------------------------------------------- 头文件整理 --
    $incDst = Join-Path $stage 'include\assimp_6_0_2'
    New-CleanDirectory $incDst
    Copy-TreeContents -Source (Join-Path $src 'include\assimp') -Destination (Join-Path $incDst 'assimp')
    # CMake 生成的头文件（config.h / revision.h）覆盖源码中的模板版本
    $generated = Join-Path (Join-Path $script:BuildDir 'assimp-release') 'include\assimp'
    foreach ($f in @('config.h', 'revision.h')) {
        $p = Join-Path $generated $f
        if (Test-Path -LiteralPath $p) { Copy-FileToDirectory -Source $p -DestinationDirectory (Join-Path $incDst 'assimp') }
    }
    Copy-FileToDirectory -Source (Join-Path $src 'LICENSE') -DestinationDirectory $incDst
    Write-Ok 'Assimp 头文件 -> include\assimp_6_0_2'

    Write-Ok 'Assimp 依赖整理完成'
}
