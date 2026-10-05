# Egret Physics

![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)
![License](https://img.shields.io/badge/license-GPLv3-green.svg)


**注意** dependency 文件夹是本项目所需要的所有第三方库文件。由于其体积过大，不再随仓库一同提交。
请使用 dependency_build 文件夹中的脚本进行自动化下载与构建


一个现代化的物理模拟引擎，支持刚体动力学、碰撞检测和约束求解。

![program_screenshot_v1.0-Simulator](./README.assets/program_screenshot_v1.0-Simulator.png)

![program_screenshot_v1.0-Editor](./README.assets/program_screenshot_v1.0-Editor.png)

![program_screenshot_v1.0-Editor_2](./README.assets/program_screenshot_v1.0-Editor_2.png)

## 特性

- **刚体动力学**：支持多种刚体形状，包括球体、立方体、圆柱体、圆环、圆盘、杆和球壳体
- **碰撞检测**：全面的碰撞检测系统，支持多种形状组合
- **约束求解**：支持约束包括支撑面、滑轨、连接线和连接杆
- **物理场**：支持重力和质心引力场
- **模拟策略**：可配置的宽阶段策略、接触求解器和积分器策略
- **Qt 界面**：现代化的 Qt Quick 用户界面，支持 3D 可视化
- **数据分析工作区**：读取 `.epsx` 仿真文件，按抽帧比抽取指定实体/通道的数据，支持
  散点图（平移 / 缩放 / 框选）、导出 xlsx 与 CSV、五类曲线拟合（线性、多项式、三角函数、
  指数、对数）以及与实际理论方程的误差对比

## 物理模型架构

以下是物理引擎的核心类继承关系图：

![物理模型继承链](./README.assets/PhysicsModel_InheritanceChain.png)

## 依赖

### 第三方库

| 库                                                   | 版本     | 许可证                        |
|-----------------------------------------------------|--------|----------------------------|
| [Boost](https://www.boost.org/)                     | 1.90.0 | Boost Software License 1.0 |
| [Eigen](https://libeigen.gitlab.io/)                | 5.0.0  | Mozilla Public License 2.0 |
| [Magic Enum](https://github.com/Neargye/magic_enum) | 0.9.8  | MIT License                |
| [Qt](https://www.qt.io/)                            | 6.9.3  | LGPLv3/GPLv3               |
| [QXlsx](https://github.com/QtExcel/QXlsx)           | 1.5.0  | MIT License                |
| [muparser](https://github.com/beltoforion/muparser) | 2.3.5  | BSD 2-Clause               |
| [QCustomPlot](https://www.qcustomplot.com/)         | 2.1.1  | GPLv3                      |

其中 muparser 用于解析理论方程表达式（误差对比），QCustomPlot 用于数据分析工作区的图表
（平移、滚轮缩放、框选放大、自适应刻度与自适应采样）。两者都以源码形式随主程序一起编译，
不产出额外的静态库。

上述依赖可以由 `dependency_build` 下的脚本自动重建（下载、解压、整理或编译），例如只重建
本次新增的两个纯源码依赖：

```powershell
pwsh -File dependency_build\Build-Dependencies.ps1 -Step muparser,qcustomplot
```

需要发布到 `dependency` 时在步骤里追加 `assemble`；`-Step all` 会重建全部依赖。

### 其他组件

| Repo                                                                       | 许可证   |
|----------------------------------------------------------------------------|-------|
| [Transform Gizmo](https://github.com/YJHScenery/QtQuick3D-Transform-Gizmo) | GPLv3 |
| [3D Coordinate](https://github.com/YJHScenery/Smart-3D-Coordinate-System)  | GPLv3 |

## 快速开始

### 前提条件

- CMake 4.2 或更高版本（若不兼容，自行修改 CMakeLists.txt，最低支持版本尚不明确）
- Qt 6.9.3（需包含 Core、Gui、Widgets、Quick、Qml、Quick3D、Sql、Quick3DAssetImport、QuickControls2、Multimedia、Concurrent、PrintSupport 模块）或兼容版本
- MSVC 2026 64位（Windows）或兼容的 C++ 编译器（C++20 或更高）

### 构建说明

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### 构建选项

| 选项            | 描述     | 默认值 |
|---------------|--------|-----|
| `BUILD_TESTS` | 构建单元测试 | ON  |

### 运行应用

构建完成后，从构建目录运行可执行文件：

```bash
# Windows
./Release/Egret_Physics.exe

# Linux（如适用）
./Egret_Physics
```

**注意** QXlsx 模块尚未提供 Linux/MacOS 平台的静态链接库支持，有需要请自行编译

## 项目结构

```
Egret Physics/
├── src/                    # 源代码
│   ├── main.cpp            # 应用入口点
│   ├── model/              # 核心物理模型
│   │   ├── physics/        # 物理引擎组件
│   │   │   ├── rigid/      # 刚体系统
│   │   │   ├── field/      # 物理场
│   │   │   └── constraints/# 约束
│   │   ├── solver/         # 模拟求解器
│   │   ├── scene/          # 场景管理
│   │   └── strategy/       # 模拟策略
│   ├── view_model/         # UI 视图模型
│   │   ├── simulator/      # 实时仿真门面
│   │   ├── simulation_serializer/ # EPSX 文件读写
│   │   ├── simulation_player/     # 离线求解与播放
│   │   └── analysis/       # 数据分析：抽取、LOD、拟合、误差对比、图表桥接
│   ├── view/               # UI 视图
│   └── utils/              # 工具函数
├── dependency/             # 第三方依赖
│   └── include/            # 头文件库
├── dependency_build/       # 第三方依赖重建脚本（可单独重建 muparser / QCustomPlot 等）
├── resources/              # 资源（图标、RC 文件）
├── tests/                  # 单元测试
└── CMakeLists.txt          # 构建配置
```

## 使用方法

### 创建物理场景

```cpp
#include "model/scene/world_scene_manager.h"
#include "model/solver/solver.h"
#include "model/strategy/integrator_strategy/semi_implicit_euler_integrator.h"
#include "model/strategy/broad_phase_strategy/brute_force_broad_phase.h"
#include "model/strategy/contact_strategy/frictionless_contact_resolver.h"

// 使用默认配置创建求解器
egret::SolverConfig config{};
auto integrator = std::make_unique<egret::SemiImplicitEulerIntegrator>();
auto broadPhase = std::make_unique<egret::BruteForceBroadPhase>();
auto contactResolver = std::make_unique<egret::FrictionlessContactResolver>();

auto solver = std::make_unique<egret::Solver>(
    config,
    std::move(integrator),
    std::move(broadPhase),
    std::move(contactResolver)
);

// 创建场景管理器
auto scene = std::make_unique<egret::WorldSceneManager>(std::move(solver));

// 添加重力场
scene->addGravityField({0, -9.81, 0}, {0, 0, 0}, "Gravity");

// 在位置 (0, 10, 0) 生成一个半径为 1.0、质量为 1.0 的球体
std::uint64_t sphereId = scene->spawnSphere("Sphere", {0, 10, 0}, {0, 0, 0}, 1.0, 1.0);

// 生成地面盒子（质量为 0 表示静态）
std::uint64_t groundId = scene->spawnBox("Ground", {0, -1, 0}, {0, 0, 0}, {10, 1, 10}, 0.0);

// 运行模拟（10 秒，60 FPS）
for (int i = 0; i < 600; ++i) {
    egret::SolverStepResult result = scene->tick(1.0 / 60.0);
    // 如有需要可访问模拟统计数据
    // std::cout << "Bodies: " << result.stats.bodyCount << std::endl;
}
```

### 添加约束

```cpp
// 创建单摆约束
// 锚点位置在 (0, 5, 0)，连接到球体
std::uint64_t pendulumId = scene->createSimplePendulum("Pendulum", 3.0, {0, 5, 0}, sphereId);

// 创建两个由线连接的球体
std::uint64_t bodyA = scene->spawnSphere("Ball A", {-2, 5, 0}, {0, 0, 0}, 0.5, 1.0);
std::uint64_t bodyB = scene->spawnSphere("Ball B", {2, 5, 0}, {0, 0, 0}, 0.5, 1.0);
std::uint64_t lineId = scene->createConnectingLine("Connector", 5.0, bodyA, bodyB);

// 启用/禁用约束
scene->setConstraintEnabled(pendulumId, false);
```

### 处理实体属性

```cpp
// 获取和设置物体位置
Eigen::Vector3d pos = scene->getBodyPosition(sphereId).value_or({0, 0, 0});
scene->setBodyPosition(sphereId, {0, 20, 0});

// 获取模拟时间和步数
double simTime = scene->getSimulationTime();
std::size_t stepCount = scene->getStepCount();

// 清空场景
scene->clear();
```

## 运行测试

运行单元测试：

```bash
cd build
ctest -C Release
```

**注意** 单元测试已暂停维护，鉴于作者对单元测试的了解极其浅薄，因此无法保证其能正常工作。

## 许可证

本项目采用 GNU General Public License v3.0 许可。详情请参见 [GPL_V3.0.md](GPL_V3.0.md)。

第三方库许可证请参见 [NOTICE](NOTICE)。

## 贡献

欢迎贡献！请遵循以下指南：

1. Fork 仓库
2. 创建特性分支（`git checkout -b feature/my-feature`）
3. 进行修改并添加测试
4. 确保所有测试通过
5. 提交拉取请求

## 代码风格

本项目遵循 [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)，以下是例外情况：

- 使用 4 个空格进行缩进（不使用制表符）
- 行长度限制为 120 字符

## 联系方式

如有问题或建议，请在仓库中提交 issue。
