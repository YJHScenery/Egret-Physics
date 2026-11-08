"""@file decompose.py
@brief 生成开销分解用的对照场景：纯物理、最小回调、纯循环负载（C 与 Lua 各一份）。
"""

import build_scenes as base
import diagnose

MIN_C = """void on_step(void)
{
    state_set_number("dist", simulation_time());
}
"""

MIN_LUA = """function on_step()
    state_set_number("dist", simulation_time())
end
"""

LOOP_C = """double g_f = 0.0;
void on_step(void)
{
    double i = 0.0;
    while (i < parameter_number("iters"))
    {
        g_f = 0.5 * g_f + 0.25 * math_sqrt(0.25 + math_abs(g_f) * 0.01);
        i = i + 1.0;
    }
    state_set_number("filter", g_f);
}
"""

LOOP_LUA = """local ITERS = parameter_number("iters")
local g_f = 0.0
function on_step()
    local i = 0.0
    while i < ITERS do
        g_f = 0.5 * g_f + 0.25 * math_sqrt(0.25 + math_abs(g_f) * 0.01)
        i = i + 1.0
    end
    state_set_number("filter", g_f)
end
"""


def main():
    """@brief 生成分解对照场景。"""
    diagnose.build_world(base.GENERAL, base.FIELDS, "phys_only.epscenex")
    diagnose.build_world(base.GENERAL, base.FIELDS, "min_c.epscenex", MIN_C, "c_wasm")
    diagnose.build_world(base.GENERAL, base.FIELDS, "min_lua.epscenex", MIN_LUA, "lua")
    diagnose.build_world(base.GENERAL, base.FIELDS, "loop_c.epscenex", LOOP_C, "c_wasm")
    diagnose.build_world(base.GENERAL, base.FIELDS, "loop_lua.epscenex", LOOP_LUA, "lua")


if __name__ == "__main__":
    main()
