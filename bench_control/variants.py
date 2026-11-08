"""@file variants.py
@brief 生成用于分解耗时的对照场景：纯物理、空控制器、以及正式控制器。

用于判断“推进与控制”耗时中物理推进与控制解释执行各占多少。
"""

import json
import pathlib
import zipfile

import build_scenes as base

ROOT = pathlib.Path(__file__).resolve().parent


def build_variant(backend, source_text, output_name, with_program=True):
    """@brief 用给定源码生成一个场景文件；with_program 为假时省略 programs。"""
    scene = {
        "version": "1.0",
        "modelCount": len(base.GENERAL) + len(base.FIELDS),
        "scene_settings": {"programs": []},
    }
    order = [0] * len(base.GENERAL) + [1] * len(base.FIELDS)
    if with_program:
        program = {
            "bindings": dict(base.BINDINGS),
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

    metadata = {"magic": "EPSCENE", "version": "2.0", "entity_order": order, "scene": scene}
    target = ROOT / output_name
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("metadata.json", json.dumps(metadata, ensure_ascii=False, indent=4))
        archive.writestr("entities/general.json", json.dumps(base.GENERAL, ensure_ascii=False, indent=4))
        archive.writestr("entities/fields.json", json.dumps(base.FIELDS, ensure_ascii=False, indent=4))
        archive.writestr("entities/constraints.json", "[]")
        if with_program:
            archive.writestr(program["source"], source_text)
    print(f"written {target}")


if __name__ == "__main__":
    build_variant("none", "", "variant_physics_only.epscenex", with_program=False)
    build_variant("c_wasm", "void on_step(void)\n{\n}\n", "variant_c_empty.epscenex")
    build_variant("lua", "-- 空控制器\nfunction on_step()\nend\n", "variant_lua_empty.epscenex")
