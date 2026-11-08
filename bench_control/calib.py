"""@file calib.py
@brief 标定两种后端的每步解释执行开销：同一纯数值循环在不同迭代次数下的耗时。
"""

import json
import pathlib
import zipfile

import build_scenes as base
import diagnose

ROOT = pathlib.Path(__file__).resolve().parent
NO_PLATFORM = [entity for entity in base.GENERAL if entity["id"] != "8"]

C_LOOP = """void on_step(void)
{
    double f = 0.0;
    double i = 0.0;
    while (i < parameter_number("iters"))
    {
        f = 0.5 * f + 0.25 * math_sqrt(0.25 + math_abs(f) * 0.01);
        i = i + 1.0;
    }
    state_set_number("filter", f);
}
"""

LUA_LOOP = """local ITERS = parameter_number("iters")
function on_step()
    local f = 0.0
    local i = 0.0
    while i < ITERS do
        f = 0.5 * f + 0.25 * math_sqrt(0.25 + math_abs(f) * 0.01)
        i = i + 1.0
    end
    state_set_number("filter", f)
end
"""


def build_with_iters(iterations, backend, source, output_name):
    """@brief 用指定迭代次数参数生成标定场景。"""
    original = base.PARAMETERS["iters"]
    base.PARAMETERS["iters"] = float(iterations)
    try:
        diagnose.build_world(NO_PLATFORM, base.FIELDS, output_name, source, backend)
    finally:
        base.PARAMETERS["iters"] = original


def main():
    """@brief 生成标定场景。"""
    for iterations in (160, 640, 2560, 5120):
        build_with_iters(iterations, "c_wasm", C_LOOP, f"calib_c_{iterations}.epscenex")
        build_with_iters(iterations, "lua", LUA_LOOP, f"calib_lua_{iterations}.epscenex")


if __name__ == "__main__":
    main()
