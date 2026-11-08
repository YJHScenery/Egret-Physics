"""@file diagnose.py
@brief 定位物理推进耗时来源：对比不同世界配置下的纯物理耗时。
"""

import json
import pathlib
import zipfile

import build_scenes as base

ROOT = pathlib.Path(__file__).resolve().parent


def build_world(general, fields, output_name, source_text=None, backend="c_wasm", bindings=None):
    """@brief 用给定世界与可选控制程序生成场景文件。"""
    order = [0] * len(general) + [1] * len(fields)
    scene = {
        "version": "1.0",
        "modelCount": len(order),
        "scene_settings": {"programs": []},
    }
    control_file = None
    if source_text is not None:
        program = {
            "bindings": dict(bindings or base.BINDINGS),
            "enabled": True,
            "fuel": 1000000,
            "name": backend,
            "parameters": dict(base.PARAMETERS),
            "period": 0,
            "source": "control/program_1." + ("lua" if backend == "lua" else "c"),
            "start": 0,
            "version": 1,
        }
        if backend == "lua":
            program["backend"] = "lua"
        scene["scene_settings"]["programs"].append(program)
        control_file = source_text

    metadata = {"magic": "EPSCENE", "version": "2.0", "entity_order": order, "scene": scene}
    target = ROOT / output_name
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("metadata.json", json.dumps(metadata, ensure_ascii=False, indent=4))
        archive.writestr("entities/general.json", json.dumps(general, ensure_ascii=False, indent=4))
        archive.writestr("entities/fields.json", json.dumps(fields, ensure_ascii=False, indent=4))
        archive.writestr("entities/constraints.json", "[]")
        if control_file is not None:
            archive.writestr(program["source"], control_file)
    print(f"written {target.name}")


def main():
    """@brief 生成诊断场景。"""
    no_platform = [entity for entity in base.GENERAL if entity["id"] != "8"]
    small_platform = [entity for entity in base.GENERAL if entity["id"] != "8"] + [
        base.box("8", "承载平台", [0.0, 0.0, -2.5], 0.0, [2.0, 2.0, 1.0], False)
    ]
    c_source = (ROOT / "scene_control.c").read_text(encoding="utf-8")

    build_world(no_platform, base.FIELDS, "diag_no_platform_physics.epscenex")
    build_world(small_platform, base.FIELDS, "diag_small_platform_physics.epscenex")
    build_world(no_platform, base.FIELDS, "diag_no_platform_c.epscenex", c_source, "c_wasm")
    build_world(small_platform, base.FIELDS, "diag_small_platform_c.epscenex", c_source, "c_wasm")


if __name__ == "__main__":
    main()
