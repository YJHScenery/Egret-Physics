<!-- @file code_editor_and_presets.md @brief C 控制编辑器与叠加预设的使用及选型说明。 -->
# C 控制编辑器与预设

控制窗口沿用 DeepBlueTheme，按钮、输入框、下拉框、程序列表、源码及诊断区均使用深蓝主题。

源码区支持 C 关键字、类型、函数、预处理指令、数值、字符串、字符常量、行注释与跨行块注释的彩色高亮；带行号、横纵向滚动、Tab 缩进及 Shift+Tab 退格缩进。它是轻量编辑器，尚不提供代码补全、语言服务器或语义诊断。编译诊断继续通过“检查当前草稿”获得。

默认字体为打包的 JetBrains Mono，默认字号为 14 px。可选择系统等宽字体并调整字号，设置保存在本机用户配置中，不写入 epscene；“默认”可恢复字体和字号。字体许可证见 `resources/fonts/OFL.txt`。

## 开源编辑器调研

检索日期：2026-11-07。

| 项目 | 接入方式与取舍 |
| --- | --- |
| [Qutepart](https://github.com/diegoiast/qutepart-cpp) | Qt 代码编辑器控件，适合 Widgets 界面；当前主界面为 Qt Quick，接入需要额外的嵌入适配。 |
| [Monaco Editor](https://github.com/microsoft/monaco-editor) | 浏览器代码编辑器；在本项目中需要引入 WebView/WebEngine 与通信桥。 |
| [KSyntaxHighlighting](https://api.kde.org/qml-org-kde-syntaxhighlighting-syntaxhighlighter.html) | 可为 QML 文本框提供高亮，支持选择语法与主题；适合作为后续多语言扩展，但会增加 KDE 构建和部署依赖。 |
| [QSyntaxHighlighter](https://doc.qt.io/qt-6/qsyntaxhighlighter.html) | 本次实际采用：复用已有 Qt 依赖，为 QML 文本文档提供 C 词法高亮，界面可直接使用现有主题组件。 |

## 添加预设

编辑器的“添加预设”按钮提供七项内置 epscene(JSON)：

- 标准重力场：沿 Z 负方向，10 m/s²，与现有教学场景约定一致。
- 大型地平面：尺寸 10000 × 10000 × 10，中心 (0, 0, -5)，质量 0，顶面 z = 0。
- 标准测试场景：重力、地面与自由落体球。
- 摩擦地面：静摩擦 0.8，动摩擦 0.6。
- 单摆：固定悬点与摆球、连接线及重力。
- 弹簧振子：固定球与振子、弹簧及重力。
- 自由落体球：不同质量的球与重力。

预设保存在 `resources/presets/`。添加时保留现有对象、控制程序和场景设置，重新分配数字 ID，重映射约束内部引用，重名对象自动追加后缀；整组添加支持一次撤销及重做。包含重力的组合预设会与已有重力累计，添加后界面会提示。

新增预设需同时登记 `resources/presets.qrc` 与 `editor_manager.cpp` 的预设清单。

## 验证

`Egret_Editor_Tests` 覆盖七项预设重复叠加、约束引用、地面参数、撤销重做、JSON 保存加载、C 高亮优先级、内置字体及控制窗口离屏加载。

实际显示和交互仍需在 Windows GUI 中检查：打开 C 控制窗口，切换程序、编辑并检查源码、改变字体字号，再添加和撤销多个预设，确认显示与仿真结果符合预期。
