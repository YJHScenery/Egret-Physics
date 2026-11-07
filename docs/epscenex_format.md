<!-- @file epscenex_format.md @brief 场景文件 V1/V2 格式与兼容性约定。 -->

# 场景文件格式

V1.0 使用 `.epscene`，内容是 UTF-8 JSON，控制器 `source` 保存 C 源码字符串。
V2.0 使用 `.epscenex`，内容是标准 ZIP，所有 JSON 和 C 文件均使用 UTF-8 编码。
编辑器默认保存 V2.0；另存为 `.epscene` 时输出 V1.0 JSON。打开旧文件后直接保存仍保留旧格式。
读取器依据内容识别格式，编辑器、模拟器与 `--solve` 共用此读取器。
内部 JSON、撤销历史和 EPSX 内嵌场景仍包含源码字符串，不依赖归档之外的文件。

## V2.0 包结构

```text
metadata.json
entities/
    general.json
    fields.json
    constraints.json
control/
    program_1.c
    program_2.c
config/
```

三个实体文件都是 JSON 数组，分别保存 `category` 为 0、1、2 的实体。
旧实体未指定 `category` 时按一般实体处理。实体字段与 V1.0 一致，包括 ID、引用和扩展负载。
`control/` 中每个程序单独存放源码，按程序顺序编号，与显示名称无关。
`config/` 当前为空，仅预留后续配置；加载时不会解包到磁盘。

## 元数据

```json
{
    "magic": "EPSCENE",
    "version": "2.0",
    "entity_order": [1, 0, 2],
    "scene": {
        "version": "1.0",
        "scene_settings": {
            "programs": [
                {
                    "version": 1,
                    "name": "控制程序",
                    "source": "control/program_1.c",
                    "bindings": {"body": "1"}
                }
            ]
        }
    }
}
```

根级 `magic` 和 `version` 共同标识归档格式，当前只接受 `EPSCENE` 和 `2.0`。
`scene` 保存原场景 JSON 除 `models` 外的字段；其中 `version: "1.0"` 指内部 JSON 数据结构，
与归档版本独立，保留旧版数据和扩展字段。
`entity_order` 按原实体列表顺序记录类别，读取时依次取对应数组的下一个实体，以还原跨类别顺序。
索引必须完整覆盖三个数组。

`scene.scene_settings.programs[*].source` 在包内改为 ZIP 根目录下的相对路径。
读取后恢复为源码字符串，再交给原有控制器编译与运行流程。
控制器的名称、开关、绑定、参数、时序及执行顺序保持原值。
归档缺少必需文件、身份或版本不匹配、实体顺序无效、源码路径无效时，读取失败且不覆盖输出场景。

## 命令行

```powershell
./build/Release/Egret_Physics.exe --solve ./scene.epscenex --duration 1 -o ./result.epsx
```

文件关联工具 `epscene_register.exe` 同时支持 `.epscene` 和 `.epscenex`，更新安装后需重新运行该工具完成注册。
