"""@file exp.py
@brief 控制回调开销分段实验：在无平台世界上分别只执行某类操作，定位耗时来源。
"""

import pathlib

import build_scenes as base
import diagnose

ROOT = pathlib.Path(__file__).resolve().parent

NO_PLATFORM = [entity for entity in base.GENERAL if entity["id"] != "8"]

PROGRAMS = {
    "exp_min": """void on_step(void)
{
    state_set_number("dist", simulation_time());
}
""",
    "exp_loop": """void on_step(void)
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
""",
    "exp_enum": """void on_step(void)
{
    Entity hub = scene_entity("hub");
    Entity sat1 = scene_entity("sat1");
    EntityCollection m = scene_entities();
    Entity e = collection_next(m);
    double c = 0.0;
    while (entity_exists(e))
    {
        c = c + 1.0;
        e = collection_next(m);
    }
    state_set_number("bodies", c);
    state_set_bool("contact", entities_touching(hub, sat1));
}
""",
    "exp_write": """void on_step(void)
{
    Entity hub = scene_entity("hub");
    Entity sat1 = scene_entity("sat1");
    Vector3 p = entity_position(hub);
    entity_set_force(hub, vector3(p.x, p.y, p.z));
    entity_set_force(sat1, vector3(p.x, p.y, p.z));
    entity_set_torque(hub, vector3(0.0, 0.0, 0.0));
    Material mm = material(0.4, 0.3, 0.2);
    entity_set_material(hub, mm);
}
""",
    "exp_states": """void on_step(void)
{
    state_set_number("a", 1.0);
    state_set_number("b", 2.0);
    state_set_number("c", 3.0);
    state_set_number("d", 4.0);
    state_set_number("e", 5.0);
    state_set_number("f", 6.0);
    state_set_number("g", 7.0);
    state_set_number("h", 8.0);
    state_set_bool("i", true);
    state_set_bool("j", false);
    state_set_number("k", 11.0);
    state_set_number("l", 12.0);
}
""",
    "exp_numbers": """void on_step(void)
{
    NumberList list = numbers_create();
    double i = 0.0;
    while (i < 24.0)
    {
        numbers_push(list, i);
        i = i + 1.0;
    }
    while (numbers_count(list) > 0U)
    {
        numbers_pop(list);
    }
    numbers_destroy(list);
}
""",
}


def main():
    """@brief 生成并打印各分段场景。"""
    for name, source in PROGRAMS.items():
        diagnose.build_world(NO_PLATFORM, base.FIELDS, name + ".epscenex", source, "c_wasm")


if __name__ == "__main__":
    main()
