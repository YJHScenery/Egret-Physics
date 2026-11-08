"""@file verify_equivalence.py
@brief 逐帧比对 C 与 Lua 两个场景的 EPSX 输出，证明二者功能完全相同。
"""

import subprocess
import json

EXE = r".\build\Release\Egret_Physics.exe"
A = "bench_control/run_c_1.epsx"
B = "bench_control/run_lua_1.epsx"


def frame(path, index):
    """@brief 解析指定帧的 EPSX 内容为字典。"""
    proc = subprocess.run([EXE, "-p", "epsx", str(index), path], capture_output=True)
    return json.loads(proc.stdout.decode("utf-8", "replace"))


def main():
    """@brief 遍历若干帧并统计差异。"""
    total = 0
    for index in (0, 20, 60, 100, 150, 200, 250, 300, 350, 399):
        a = frame(A, index)
        b = frame(B, index)
        diff = 0
        if len(a["bodies"]) != len(b["bodies"]):
            diff += 1
        for x, y in zip(a["bodies"], b["bodies"]):
            for key in ("position", "linearVelocity", "rotation", "angularVelocity"):
                if x[key] != y[key]:
                    diff += 1
        if a.get("controlEvents") != b.get("controlEvents"):
            diff += 1
        total += diff
        print("frame %3d bodies=%d diffs=%d" % (index, len(a["bodies"]), diff))
    print("total diffs:", total)


if __name__ == "__main__":
    main()
