"""@file build_scenes.py
@brief 生成控制后端对比基准的两个 .epscenex 场景文件。

两个场景除控制程序源码与 backend 字段外完全相同：同一世界、同一绑定、同一参数、
同一周期与 fuel 预算。输出：
  - control_arena_c.epscenex   （受限 C / Wasm 后端）
  - control_arena_lua.epscenex （受限 Lua 后端）
"""

import json
import pathlib
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent
OUT = ROOT.parent / "bench_control"

# 场景参数：两个场景完全一致。
PARAMETERS = {
    "kp": 25.0,
    "kd": 7.0,
    "maxf": 25.0,
    "radius": 1.4,
    "gain": 3.0,
    "satkp": 8.0,
    "satkd": 3.0,
    "satmax": 12.0,
    "speedcap": 100.0,
    "iters": 8000.0,
    "torque": 0.5,
}

BINDINGS = {
    "hub": "1",
    "sat1": "2",
    "sat2": "3",
    "sat3": "4",
    "sat4": "5",
    "ball": "6",
}


def box(entity_id, name, pos, mass, size, rotation_enabled, velocity=None):
    """构造一个box实体字典。"""
    return {
        "category": 0,
        "type": 1,
        "id": entity_id,
        "name": name,
        "pos": pos,
        "mass": mass,
        "box_size": size,
        "rotation_enabled": rotation_enabled,
        "initial_velocity": velocity or [0.0, 0.0, 0.0],
        "initial_angular_velocity": [0.0, 0.0, 0.0],
        "rotation": [1.0, 0.0, 0.0, 0.0],
        "scale": [1.0, 1.0, 1.0],
        "restitution": 0.2,
        "static_friction": 0.4,
        "dynamic_friction": 0.3,
        "visible": True,
    }


def sphere(entity_id, name, pos, mass, radius, velocity):
    """构造一个球体实体字典。"""
    return {
        "category": 0,
        "type": 8,
        "id": entity_id,
        "name": name,
        "pos": pos,
        "mass": mass,
        "radius": radius,
        "initial_velocity": velocity,
        "initial_angular_velocity": [0.0, 0.0, 0.0],
        "rotation": [1.0, 0.0, 0.0, 0.0],
        "scale": [1.0, 1.0, 1.0],
        "restitution": 0.45,
        "static_friction": 0.4,
        "dynamic_friction": 0.3,
        "visible": True,
    }


GENERAL = [
    box("1", "中心体", [0.0, 0.0, 3.4], 2.0, [1.2, 1.2, 1.2], True),
    box("2", "卫星甲", [1.4, 0.0, 3.4], 1.0, [0.4, 0.4, 0.4], True),
    box("3", "卫星乙", [0.0, 1.4, 3.4], 1.0, [0.4, 0.4, 0.4], True),
    box("4", "卫星丙", [-1.4, 0.0, 3.4], 1.0, [0.4, 0.4, 0.4], True),
    box("5", "卫星丁", [0.0, -1.4, 3.4], 1.0, [0.4, 0.4, 0.4], True),
    sphere("6", "自由飞行球", [2.5, 0.0, 5.2], 1.5, 0.35, [-0.8, 0.0, 0.5]),
]

FIELDS = []

CONSTRAINTS = []


def build(backend, source_name, output_name):
    """打包一个场景文件。"""
    source_path = ROOT / source_name
    source_text = source_path.read_text(encoding="utf-8")

    program = {
        "bindings": dict(BINDINGS),
        "enabled": True,
        "fuel": 1000000,
        "name": "协同编队控制器",
        "parameters": dict(PARAMETERS),
        "period": 0,
        "source": "control/program_1." + ("lua" if backend == "lua" else "c"),
        "start": 0,
        "version": 1,
    }
    if backend == "lua":
        program["backend"] = "lua"

    order = [0] * len(GENERAL) + [1] * len(FIELDS) + [2] * len(CONSTRAINTS)
    metadata = {
        "magic": "EPSCENE",
        "version": "2.0",
        "entity_order": order,
        "scene": {
            "version": "1.0",
            "modelCount": len(order),
            "timestamp": "2026-11-08T15:30:00",
            "scene_settings": {"programs": [program]},
        },
    }

    target = OUT / output_name
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("metadata.json", json.dumps(metadata, ensure_ascii=False, indent=4))
        archive.writestr("entities/general.json", json.dumps(GENERAL, ensure_ascii=False, indent=4))
        archive.writestr("entities/fields.json", json.dumps(FIELDS, ensure_ascii=False, indent=4))
        archive.writestr("entities/constraints.json", json.dumps(CONSTRAINTS, ensure_ascii=False, indent=4))
        archive.writestr(program["source"], source_text)
    print(f"written {target} ({target.stat().st_size} bytes)")


if __name__ == "__main__":
    build("c_wasm", "scene_control.c", "control_arena_c.epscenex")
    build("lua", "scene_control.lua", "control_arena_lua.epscenex")
