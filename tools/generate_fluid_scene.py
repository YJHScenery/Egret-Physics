#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成「大量小球近似流体」演示场景的 JSON 文件。

文件编码约定（与工程内 EditorManager::saveToJson / QJsonDocument::toJson 一致）：
  * UTF-8 编码，不带 BOM；
  * LF 换行（0x0A），不写 CR；
  * 顶层为对象，键为 modelCount / models / timestamp / version；
  * 4 空格缩进，即 QJsonDocument::Indented 风格。

场景构成：
  * 5 个质量为 0 的静态扁长方体：1 块底板 + 4 面侧壁，拼成一个上端敞口的容器；
  * 容器内腔自底面起、占据下半段高度的区域内，按简单立方点阵均匀填充小球，
    相邻小球之间保留 SPHERE_GAP 的表面间距（初始状态互不接触）；
  * 1 个重力场（category = 1，type = 1）。

用法：
    python tools/generate_fluid_scene.py [每边球数] [输出路径]
    python tools/generate_fluid_scene.py <nx> <ny> <nz> [输出路径]

默认按 30 x 30 x 10 排列，共 9,000 个小球；输出路径默认为 demo/demo_fluid.json。
"""

import sys
import time
from pathlib import Path

# ---------------------------------------------------------------------------
#  场景参数（长度单位为项目内部的场景单位，与 demo/demo_world.json 一致）
# ---------------------------------------------------------------------------

SPHERE_RADIUS = 10.0            # 小球半径
SPHERE_GAP = 5.0                # 相邻小球表面之间的间距（初始不接触）
SPHERE_MASS = 10.0              # 小球质量
RESTITUTION = 1.0               # 恢复系数，与参考场景保持一致

WALL_THICKNESS = 50.0           # 容器壁厚度（"扁"长方体）
WALL_CLEARANCE = 5.0            # 球体最外缘与容器内壁之间的初始间隙
FILL_HEIGHT_RATIO = 2.0         # 容器内腔高度 / 小球填充高度（即小球占据内腔下半段）

SPACING = SPHERE_RADIUS * 2.0 + SPHERE_GAP   # 相邻球心间距

GRAVITY = (0.0, 0.0, -180.0)    # 重力场矢量，与参考场景保持一致

# 默认的排列规模：30 x 30 x 10 = 9,000 个小球
DEFAULT_SHAPE = (30, 30, 10)


def num(value: float) -> str:
    """把数值格式化为 JSON 字面量：整数值不写小数点。"""
    f = float(value)
    if f.is_integer():
        return str(int(f))
    return repr(f)


def vec(x: float, y: float, z: float) -> str:
    """格式化为 JSON 三元数组。"""
    return "[%s, %s, %s]" % (num(x), num(y), num(z))


def box_entry(entity_id: int, name: str, pos, size) -> str:
    """生成一个静态长方体（容器构件）的模型条目文本。"""
    return (
        "        {\n"
        '            "category": 0,\n'
        '            "type": 1,\n'
        '            "id": "%d",\n'
        '            "name": "%s",\n'
        '            "pos": %s,\n'
        '            "initial_velocity": [0, 0, 0],\n'
        '            "initial_angular_velocity": [0, 0, 0],\n'
        '            "rotation": [1, 0, 0, 0],\n'
        '            "scale": [1, 1, 1],\n'
        '            "mass": 0,\n'
        '            "restitution": %s,\n'
        '            "box_size": %s,\n'
        '            "visible": true\n'
        "        }"
    ) % (
        entity_id,
        name,
        vec(*pos),
        num(RESTITUTION),
        vec(*size),
    )


def sphere_entry(entity_id: int, x: float, y: float, z: float) -> str:
    """生成一个小球（刚体）的模型条目文本。"""
    return (
        "        {\n"
        '            "category": 0,\n'
        '            "type": 8,\n'
        '            "id": "%d",\n'
        '            "name": "ball",\n'
        '            "pos": %s,\n'
        '            "initial_velocity": [0, 0, 0],\n'
        '            "initial_angular_velocity": [0, 0, 0],\n'
        '            "rotation": [1, 0, 0, 0],\n'
        '            "scale": [1, 1, 1],\n'
        '            "mass": %s,\n'
        '            "restitution": %s,\n'
        '            "radius": %s,\n'
        '            "visible": true\n'
        "        }"
    ) % (
        entity_id,
        vec(x, y, z),
        num(SPHERE_MASS),
        num(RESTITUTION),
        num(SPHERE_RADIUS),
    )


def gravity_entry(entity_id: int) -> str:
    """生成重力场的模型条目文本。"""
    return (
        "        {\n"
        '            "category": 1,\n'
        '            "type": 1,\n'
        '            "id": "%d",\n'
        '            "name": "重力场",\n'
        '            "pos": [0, 0, 0],\n'
        '            "field_data": {\n'
        '                "gravity": %s,\n'
        '                "reference_point": [0, 0, 0]\n'
        "            }\n"
        "        }"
    ) % (entity_id, vec(*GRAVITY))


def container_entries(shape):
    """产出 5 个静态长方体：1 块底板 + 4 面侧壁，构成上端敞口的容器。"""
    # 内腔半宽：让最外层球面与内壁之间留出 WALL_CLEARANCE 的初始间隙
    nx, ny, nz = shape
    inner_half_x = (nx - 1) * SPACING / 2.0 + SPHERE_RADIUS + WALL_CLEARANCE
    inner_half_y = (ny - 1) * SPACING / 2.0 + SPHERE_RADIUS + WALL_CLEARANCE
    outer_half_x = inner_half_x + WALL_THICKNESS
    outer_half_y = inner_half_y + WALL_THICKNESS
    wall_center_x = inner_half_x + WALL_THICKNESS / 2.0
    wall_center_y = inner_half_y + WALL_THICKNESS / 2.0
    # 填充高度 = 最上层球的顶面高度；内腔高度取它的 FILL_HEIGHT_RATIO 倍，
    # 于是小球恰好占据内腔自底面起的下半段。
    wall_height = ((nz - 1) * SPACING + 2.0 * SPHERE_RADIUS) * FILL_HEIGHT_RATIO

    # 底板：上表面恰好位于 z = 0
    yield box_entry(1, "container_floor",
                    (0.0, 0.0, -WALL_THICKNESS / 2.0),
                    (outer_half_x * 2.0, outer_half_y * 2.0, WALL_THICKNESS))
    # 沿 Y 方向延伸的两面侧壁（法线朝 X），外缘与底板外缘齐平
    yield box_entry(2, "container_wall_x_min",
                    (-wall_center_x, 0.0, wall_height / 2.0),
                    (WALL_THICKNESS, outer_half_y * 2.0, wall_height))
    yield box_entry(3, "container_wall_x_max",
                    (wall_center_x, 0.0, wall_height / 2.0),
                    (WALL_THICKNESS, outer_half_y * 2.0, wall_height))
    # 沿 X 方向延伸的两面侧壁（法线朝 Y），长度只覆盖内腔，避免与上面两面壁重叠
    yield box_entry(4, "container_wall_y_min",
                    (0.0, -wall_center_y, wall_height / 2.0),
                    (inner_half_x * 2.0, WALL_THICKNESS, wall_height))
    yield box_entry(5, "container_wall_y_max",
                    (0.0, wall_center_y, wall_height / 2.0),
                    (inner_half_x * 2.0, WALL_THICKNESS, wall_height))


def ball_entries(shape):
    """按层产出小球条目：自容器内腔底面起，逐层填满下半段高度。"""
    nx, ny, nz = shape
    half_x = (nx - 1) * SPACING / 2.0
    half_y = (ny - 1) * SPACING / 2.0
    entity_id = 6
    for layer in range(nz):
        z = SPHERE_RADIUS + layer * SPACING
        for row in range(ny):
            y = -half_y + row * SPACING
            for column in range(nx):
                x = -half_x + column * SPACING
                yield sphere_entry(entity_id, x, y, z)
                entity_id += 1


def write_scene(out_path: Path, shape) -> int:
    """流式写出场景 JSON，避免在内存中拼接超大字符串；返回模型总数。"""
    total_models = 5 + shape[0] * shape[1] * shape[2] + 1

    with out_path.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write("{\n")
        handle.write('    "modelCount": %d,\n' % total_models)
        handle.write('    "models": [\n')

        first = True
        for text in container_entries(shape):
            handle.write(("" if first else ",\n") + text)
            first = False
        for text in ball_entries(shape):
            handle.write(("" if first else ",\n") + text)
            first = False
        handle.write(("" if first else ",\n") + gravity_entry(10000000))

        handle.write("\n    ],\n")
        handle.write('    "timestamp": "%s",\n' % time.strftime("%Y-%m-%dT%H:%M:%S"))
        handle.write('    "version": "1.0"\n')
        handle.write("}\n")

    return total_models


if __name__ == "__main__":
    # 位置参数：1~3 个整数表示每边的球数，其后可跟输出路径
    numbers = []
    while len(sys.argv) > 1 and sys.argv[1].isdigit():
        numbers.append(int(sys.argv.pop(1)))
    if len(numbers) == 1:
        shape = (numbers[0], numbers[0], numbers[0])
    elif len(numbers) == 2:
        shape = (numbers[0], numbers[1], 1)
    elif len(numbers) >= 3:
        shape = (numbers[0], numbers[1], numbers[2])
    else:
        shape = DEFAULT_SHAPE

    out_file = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("demo") / "demo_fluid.json"
    start = time.perf_counter()
    count = write_scene(out_file, shape)
    megabytes = out_file.stat().st_size / (1024.0 * 1024.0)
    print("shape: %d x %d x %d" % shape)
    print("output: %s" % out_file)
    print("models: %d, size: %.1f MB, elapsed: %.1f s"
          % (count, megabytes, time.perf_counter() - start))
