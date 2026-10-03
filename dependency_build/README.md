# dependency 目录重建工具

`dependency/` 中存放的是本项目使用的第三方库（Boost、Eigen、magic_enum、CLI11、FFmpeg、
Assimp、QXlsx），其中包含体积较大的头文件与二进制文件，因此不随仓库提交。

本目录下的脚本用于在**全新克隆的仓库**中重建出与开发环境一致的 `dependency/` 目录：
从各库的官方地址下载源码包或预编译包，使用本机工具链编译需要编译的部分（Assimp、QXlsx），
最后按固定布局整理并发布到项目根目录的 `dependency/`。

## 目录结构

```
dependency_build/
├── Build-Dependencies.ps1        # 入口脚本（推荐）
├── README.md                     # 本文档
├── config/
│   └── dependencies.psd1         # 依赖版本与官方下载源（数据文件）
├── scripts/
│   ├── Common.ps1                # 公共函数：下载、解压、工具链探测、MSVC 环境
│   ├── Steps.Baseline.ps1        # 记录现有 dependency 的基线清单
│   ├── Steps.Fetch.ps1           # 工具链检测 + 下载
│   ├── Steps.Headers.ps1         # Boost / Eigen / magic_enum / CLI11
│   ├── Steps.FFmpeg.ps1          # FFmpeg 9.0.2（官方 shared 预编译包 + 源码 presets）
│   ├── Steps.Assimp.ps1          # Assimp 6.0.2 编译
│   ├── Steps.QXlsx.ps1           # QXlsx(Qt6) 1.5.0 编译
│   ├── Steps.Assemble.ps1        # 汇总发布到 dependency/
│   └── Steps.Verify.ps1          # 与基线清单比对
├── baseline/
│   └── dependency_manifest.csv   # 基线清单（相对路径 / 大小 / SHA256）
└── work/                         # 全部中间产物（下载、解压、构建、备份），可随时删除
```

## 前置条件

| 依赖 | 说明 |
| --- | --- |
| Windows + PowerShell 5.1 及以上 | 脚本本身运行环境 |
| Visual Studio 2026（或 2022） | 需包含“使用 C++ 的桌面开发”与 MSVC x64 工具集 |
| CMake ≥ 3.22、Ninja | 用于编译 Assimp 与 QXlsx；Visual Studio 自带版本会被优先使用 |
| Qt 6.9.3 msvc2022_64 | 仅编译 QXlsx 时需要，默认路径 `D:\Qt\6.9.3\msvc2022_64`，可用 `QT_DIR` 覆盖 |
| 网络访问 | 需要访问各库官方下载地址（Boost 官方镜像、GitLab、GitHub、ffmpeg.org、gyan.dev） |

> 不需要单独安装 7-Zip、也不需要 MinGW：解压使用系统自带的 `tar.exe`（bsdtar，支持 7z/tar.xz），
> FFmpeg 直接使用官方发布的 Windows shared 预编译包（版本串 `9.0.2-full_build`）。

## 快速开始

```powershell
# 一条命令完成：工具链检测 -> 下载 -> 整理头文件 -> FFmpeg -> 编译 Assimp -> 编译 QXlsx -> 发布 -> 校验
pwsh -NoProfile -ExecutionPolicy Bypass -File dependency_build\Build-Dependencies.ps1 -Step all
```

首次运行会下载约 250 MB 的依赖包，并编译 Assimp（Release + Debug）与 QXlsx（Release + Debug），
整体耗时取决于网络与 CPU，通常在 20~40 分钟。

若当前**仍有可用的 dependency 目录**，建议先记录基线，便于重建后逐项核对：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File dependency_build\Build-Dependencies.ps1 -Step baseline
```

## 步骤说明

| 步骤 | 作用 |
| --- | --- |
| `baseline` | 扫描现有 `dependency/`，生成 `baseline/dependency_manifest.csv`（路径 / 大小 / SHA256） |
| `check` | 检测 Visual Studio、CMake、Ninja、tar、curl、Qt，输出可用性报告 |
| `fetch` | 下载各依赖包到 `work/downloads`，记录实际下载源与 SHA256（`work/sources.lock.json`） |
| `headers` | 解压整理 Boost、Eigen、magic_enum、CLI11 |
| `ffmpeg` | 解压 FFmpeg 官方 shared 包，整理 `bin / lib / include` 与 `preset/ffmpeg_preset` |
| `assimp` | 编译 Assimp 6.0.2（Release + Debug），整理 DLL / 导入库 / 头文件 |
| `qxlsx` | 编译 QXlsx(Qt6) 1.5.0 静态库（Release + Debug） |
| `assemble` | 完整性检查后发布到 `dependency/`（旧目录自动备份到 `work/backup`） |
| `verify` | 把目标目录与基线清单比对，输出 `verification_report.md` |
| `build` | 等价于 `check,fetch,headers,ffmpeg,assimp,qxlsx` |
| `all` | 等价于全部步骤 |
| `clean` | 删除 `work/` 下的全部中间产物（已发布的 `dependency/` 不受影响） |

可以按需组合，例如只重建头文件与 FFmpeg 并发布：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File dependency_build\Build-Dependencies.ps1 -Step headers,ffmpeg,assemble
```

常用参数：

| 参数 | 说明 |
| --- | --- |
| `-Force` | 忽略缓存：重新下载、重新解压、重新编译 |
| `-OutputDir <path>` | 发布目录，默认 `<项目根>\dependency`；可用于先发布到临时目录做比对 |
| `-BaselineSource <path>` | `baseline` 步骤扫描的目录，默认 `<项目根>\dependency` |
| `-Jobs <n>` | 编译并行度，默认由 CMake/Ninja 自动决定 |

## 产物布局（与项目 CMakeLists 的约定一致）

| 依赖 | 版本 | 产物位置 | 来源 |
| --- | --- | --- | --- |
| Boost | 1.90.0 | `include/boost_1_90_0/{boost,LICENSE_1_0.txt}` | 官方源码包，仅取头文件 |
| Eigen | 5.0.0 | `include/eigen_5_0_0/{Eigen,unsupported,COPYING.*,LICENSE}` | 官方源码包，仅取头文件 |
| magic_enum | 0.9.8 | `include/magic_enum_0.9.8/{magic_enum,LICENSE}` | GitHub Release 源码包 |
| CLI11 | 2.6.2 | `include/CLI11/CLI11.hpp` | GitHub Release 单头文件 |
| FFmpeg | 9.0.2 | `include/ffmpeg_9_0_2/`、`lib/ffmpeg/`、`bin/ffmpeg/` | gyan.dev 官方 `9.0.2-full_build-shared` |
| FFmpeg presets | 9.0.2 | `preset/ffmpeg_preset/*.ffpreset` | FFmpeg 官方源码包 |
| Assimp | 6.0.2 | `include/assimp_6_0_2/`、`lib/Assimp/assimp-vc143-mt(d).lib`、`bin/Assimp/assimp-vc143-mt(d).dll` | 源码编译（MSVC x64） |
| QXlsx(Qt6) | 1.5.0 | `include/qxlsx_qt6_1.5.0/`、`lib/QXlsxQt6/QXlsxQt6(d).lib` | 源码编译（Qt 6.9.3 + MSVC x64） |

## 校验与回滚

```powershell
# 与基线比对（默认校验 <项目根>\dependency）
pwsh -NoProfile -ExecutionPolicy Bypass -File dependency_build\Build-Dependencies.ps1 -Step verify

# 先发布到临时目录再比对，确认无误后再正式发布
pwsh -NoProfile -ExecutionPolicy Bypass -File dependency_build\Build-Dependencies.ps1 -Step assemble -OutputDir D:\tmp\dependency_new
pwsh -NoProfile -ExecutionPolicy Bypass -File dependency_build\Build-Dependencies.ps1 -Step verify  -OutputDir D:\tmp\dependency_new
```

`assemble` 在覆盖已有 `dependency/` 之前，会把它整体移动到 `work/backup/dependency_<时间戳>`；
如需回滚，执行：

```powershell
Move-Item -LiteralPath "dependency_build\work\backup\dependency_20260101_120000" -Destination "dependency" -Force
```

## 注意事项

* **二进制不会与原有文件字节一致**：Assimp / QXlsx 由本机编译器重新生成，MSVC 补丁版本、
  时间戳、PDB 路径等都会影响哈希，因此 `verify` 比对的是“文件集合与文件大小”，
  而不是 SHA256 等值；头文件类依赖则与官方发布内容逐字节一致。
* **FFmpeg 使用官方预编译包**：在 Windows 上从源码编译 FFmpeg 需要 MSYS2 + MinGW 工具链，
  为保持与本仓库既有产物一致（`9.0.2-full_build-shared`），脚本直接使用官方发布的共享库。
* **下载源可替换**：所有地址集中在 `config/dependencies.psd1`，若某个域不可达，
  可在候选列表中追加镜像地址；脚本会按顺序尝试并记录实际使用的源。
* **可离线复用**：下载缓存与解压结果保存在 `work/` 下，重复执行不会重复下载；
  换机时只需拷贝 `work/downloads` 与 `work/sources.lock.json` 即可跳过下载。
* **密钥/授权**：QXlsx、Assimp 为宽松开源许可，FFmpeg 官方 Windows 构建为 GPL 版本，
  使用与分发时请遵守各自许可证（详见各库自带的 LICENSE 文件）。

## 重建结果与已知差异（实测记录）

编写本工具时对当前开发环境做过一次完整重建，并以重建前扫描得到的
`baseline/dependency_manifest.csv`（17094 个文件 / 477.7 MB）为基准逐文件比对：

| 比对结果 | 数量 | 说明 |
| --- | --- | --- |
| 文件集合 | 17094 / 17094 | 无缺失、无多余，目录结构完全一致 |
| 内容完全一致 | 16701 | Boost / Eigen / CLI11 / FFmpeg 等逐字节一致 |
| 仅行尾不同（CRLF/LF） | 357 | 见差异说明 1 |
| 内容不同的头文件 | 31 | 见差异说明 2 |
| 内容不同的二进制 | 5 | 见差异说明 3 |

**差异说明 1 —— 行尾**：现有目录中的 Eigen 文本文件（357 个）与 magic_enum 的 1 个文件是
CRLF 行尾，而官方源码包是 LF。脚本保持官方源码的原始字节，`verify` 使用“行尾归一化哈希”
识别这类差异，不会误判为内容不一致。如需与旧目录严格一致，可在发布后用编辑器/Git 批量转换。

**差异说明 2 —— Assimp / magic_enum 头文件来自更新的上游快照**：现有目录里 Assimp 的
30 个头文件与 magic_enum 的 `magic_enum_all.hpp` 具有比官方发布标签更新的写法
（例如 LICENSE 版权年份为 2006-2025、部分类使用 `final` / `= default`、`constexpr`，
以及 `#include "../xxx.h"` 形式的相对包含）。脚本固定使用官方发布标签
（Assimp v6.0.2、magic_enum v0.9.8），因此这些文件存在少量源码级差异；
由于头文件与同批编译出的二进制来自同一份源码，二者完全自洽，不影响使用。
若需要严格复刻某个特定提交，可在 `config/dependencies.psd1` 中把 `Archives` 换成
该提交的归档地址，例如 `https://github.com/assimp/assimp/archive/<commit-sha>.tar.gz`。

**差异说明 3 —— 自编译二进制**：`assimp-vc143-mt(d).dll/.lib`、`QXlsxQt6(d).lib`
由本机 MSVC 重新编译，编译器版本、时间戳、调试路径均不同，逐字节一致既不可能也无必要；
其导出接口、导入库名称与运行时依赖与原有文件一致。

完整的逐文件差异清单见仓库根目录旁生成的 `dependency_build/verification_report.md`。

## 常见问题

* **修改源码后重新编译**：MSVC 中文诊断信息会导致 Ninja 无法解析 `/showIncludes`，
  增量依赖跟踪可能失效（表现为 “ninja: no work to do”）。需要强制重编时加 `-Force`。
* **构建日志**：所有构建/下载步骤的日志写入 `dependency_build/work/logs/build.log`。
* **编译输出较长**：脚本已经过滤 MSVC 的“包含文件”列表；如果仍觉得输出过多，
  可只关注 `[信息] / [完成] / [警告] / [失败]` 前缀的行。
