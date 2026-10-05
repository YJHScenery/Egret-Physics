<#
    Egret Physics —— dependency 重建所使用的依赖清单（数据文件）。

    说明：
      * Version  记录的版本号与现有 dependency 目录中的版本保持一致；
      * Archives 是按优先级排列的下载源，脚本会依次尝试，第一个成功的源会被写入
        work\sources.lock.json 并记录 SHA256，之后每次运行都会校验该哈希；
      * 所有包均为官方发布地址（官方镜像 / 官方 GitHub Release / 官方源码归档）。
#>
@{
    # ------------------------------------------------------------ 头文件库 --
    Boost = @{
        Version  = '1.90.0'
        Archives = @(
            @{ Url = 'https://archives.boost.io/release/1.90.0/source/boost_1_90_0.7z';  File = 'boost_1_90_0.7z' }
            @{ Url = 'https://archives.boost.io/release/1.90.0/source/boost_1_90_0.zip'; File = 'boost_1_90_0.zip' }
        )
    }

    Eigen = @{
        Version  = '5.0.0'
        Archives = @(
            @{ Url = 'https://gitlab.com/libeigen/eigen/-/archive/5.0.0/eigen-5.0.0.tar.gz';                File = 'eigen-5.0.0.tar.gz' }
            @{ Url = 'https://codeload.github.com/libeigen/eigen/tar.gz/refs/tags/5.0.0';                  File = 'eigen-5.0.0-github.tar.gz' }
        )
    }

    MagicEnum = @{
        Version  = '0.9.8'
        Archives = @(
            @{ Url = 'https://github.com/Neargye/magic_enum/archive/refs/tags/v0.9.8.tar.gz';              File = 'magic_enum-0.9.8.tar.gz' }
            @{ Url = 'https://codeload.github.com/Neargye/magic_enum/tar.gz/refs/tags/v0.9.8';             File = 'magic_enum-0.9.8-codeload.tar.gz' }
        )
    }

    Cli11 = @{
        Version  = '2.6.2'
        Archives = @(
            @{ Url = 'https://github.com/CLIUtils/CLI11/releases/download/v2.6.2/CLI11.hpp';               File = 'CLI11-2.6.2.hpp' }
            @{ Url = 'https://raw.githubusercontent.com/CLIUtils/CLI11/v2.6.2/include/CLI11/CLI11.hpp';    File = 'CLI11-2.6.2.hpp' }
        )
    }

    # ------------------------------------------------------------- FFmpeg --
    FFmpeg = @{
        Version = '9.0.2'
        # 官方 Windows 预编译 shared 构建（与现有 dependency 中的 9.0.2-full_build 完全对应）
        Prebuilt = @(
            @{ Url = 'https://www.gyan.dev/ffmpeg/builds/packages/ffmpeg-9.0.2-full_build-shared.7z';      File = 'ffmpeg-9.0.2-full_build-shared.7z' }
        )
        # 源码包：仅用于取 presets\*.ffpreset（dependency\preset\ffmpeg_preset）
        Source = @(
            @{ Url = 'https://ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz';                                    File = 'ffmpeg-9.0.2.tar.xz' }
            @{ Url = 'https://git.ffmpeg.org/gitweb/ffmpeg.git/snapshot/ffmpeg-9.0.2.tar.gz';              File = 'ffmpeg-9.0.2-git.tar.gz' }
        )
    }

    # ------------------------------------------------- 需要编译的第三方库 --
    Assimp = @{
        Version  = '6.0.2'
        Archives = @(
            @{ Url = 'https://github.com/assimp/assimp/archive/refs/tags/v6.0.2.tar.gz';                   File = 'assimp-6.0.2.tar.gz' }
            @{ Url = 'https://codeload.github.com/assimp/assimp/tar.gz/refs/tags/v6.0.2';                  File = 'assimp-6.0.2-codeload.tar.gz' }
        )
    }

    QXlsx = @{
        Version  = '1.5.0'
        Archives = @(
            @{ Url = 'https://github.com/QtExcel/QXlsx/archive/refs/tags/v1.5.0.tar.gz';                   File = 'QXlsx-1.5.0.tar.gz' }
            @{ Url = 'https://codeload.github.com/QtExcel/QXlsx/tar.gz/refs/tags/v1.5.0';                  File = 'QXlsx-1.5.0-codeload.tar.gz' }
        )
    }

    # ------------------------------------- 纯源码依赖（随主程序一起编译） --
    MuParser = @{
        Version  = '2.3.5'
        Archives = @(
            @{ Url = 'https://github.com/beltoforion/muparser/archive/refs/tags/v2.3.5.tar.gz';      File = 'muparser-2.3.5.tar.gz' }
            @{ Url = 'https://codeload.github.com/beltoforion/muparser/tar.gz/refs/tags/v2.3.5';    File = 'muparser-2.3.5-codeload.tar.gz' }
        )
    }

    QCustomPlot = @{
        Version  = '2.1.1'
        Archives = @(
            @{ Url = 'https://www.qcustomplot.com/release/2.1.1/QCustomPlot-source.tar.gz';          File = 'QCustomPlot-2.1.1-source.tar.gz' }
        )
    }

    # ------------------------------------------------------ 本机工具链信息 --
    Qt = @{
        Version   = '6.9.3'
        Preferred = 'D:/Qt/6.9.3/msvc2022_64'
    }

    VisualStudio = @{
        Version   = '2026 (v18)'
        Preferred = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'
    }
}
