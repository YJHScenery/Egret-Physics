# 已知 ISSUES 

## 算法

- 碰撞检测算法（C++20 模块：collision）存在小于 `1e-4` 每分钟的误差
- 实体平面平行接触存在异常抖动与不平衡情况。原因可能在于 EPA 算法处理。

## 跨平台与跨编译器支持

- C++20 模块无法在低版本的 MSVC 编译器，以及 Clang、GCC 等编译器中正常工作。需要改为头文件。
- 仓库中仅保留一份 QXlsx、Assimp 的 Windows 链接库，跨平台需要自行编译
- 模块无法在低版本 CMake 中工作 

## 操作系统与内存

- **工作目录敏感的资源加载会崩溃（2026-10-02 实测）**：`SceneManagerViewModel::rebuildDemoWorld()`
  通过相对路径 `../tools/icosahedron.obj` 加载示范场景的网格（`scene_manager.cpp:1297`）。
  当进程工作目录不是「项目根目录的下一级」（例如直接在项目根目录运行
  `build-verify\Release\Egret_Physics.exe`）时，该文件找不到，网格为空指针，
  随后启动过程以 `0xC0000005`（访问冲突）崩溃，且日志里只有一行
  `Cannot Load Mesh File: ../tools/icosahedron.obj`，没有任何 QML 或 C++ 报错。
  规避：从构建目录启动（`cd build-verify; .\Release\Egret_Physics.exe`）。
  待定修法：加载失败时给出明确错误并跳过该实体，或改用 `QCoreApplication::applicationDirPath()`
  定位资源。

## 命令行

- **CLI11 实参传递错误（2026-10-02 已修复）**：`command_line.cpp` 原先把「正序、含程序名」的
  `std::vector<std::string>` 传给 `CLI::App::parse`，而该重载要求「逆序、不含程序名」，
  导致 `--solve` / `--parse` / `--play` 的选项取值整体错位，报
  `Could not convert: --duration = --solve`。只有 `--help`（不依赖取值）看起来正常。
  现已改为倒序构建并丢弃程序名，实测 `--solve` / `--parse eps <n>` / `--help` 均正常。

## 架构

- 尚不明确

## UI

- 尚不明确
