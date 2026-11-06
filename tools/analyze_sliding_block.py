"""@file analyze_sliding_block.py
@brief 提取本题 EPSX 的逐帧数据，输出 CSV、事件时刻及解析解对比。
@details 仅支持当前未压缩的 EPSX v1；文件完整性请同时用程序的 --parse epsx 检查。
"""
import argparse
import csv
import json
import struct
from pathlib import Path


def read_frames(path):
    """@brief 按格式中的块长和记录步长读取仿真时刻、物体位置及速度。"""
    raw = path.read_bytes()
    assert raw[:8] == b"\x89EPSX\r\n\x1a", "EPSX 魔数错误"
    offset = 64
    while offset + 24 <= len(raw):
        tag, flags, size, checksum, reserved = struct.unpack_from("<4sIQII", raw, offset)
        begin, end = offset + 24, offset + 24 + size
        assert end <= len(raw), "数据块越界"
        if tag == b"FRAM":
            index, flags, time_ns, step = struct.unpack_from("<IIQQ", raw, begin)
            section, bodies = begin + 24, {}
            while section + 32 <= end:
                kind, count, stride, flags, length, checksum, version = struct.unpack_from(
                    "<4sIIIQII", raw, section)
                data = section + 32
                assert data + length <= end, "帧段越界"
                if kind == b"BODY":
                    assert stride == 144 and count * stride == length, "不支持的刚体记录"
                    for i in range(count):
                        values = struct.unpack_from("<QII15dQ", raw, data + i * stride)
                        bodies[values[0]] = values[3:18]
                section = data + length
            yield time_ns * 1e-9, bodies
        offset = (end + 7) // 8 * 8


def analyze(path):
    """@brief 根据实际几何分离与首次速度归零提取事件，并验证二维性和解析值。"""
    raw = path.read_bytes()
    scene_offset = struct.unpack_from("<Q", raw, 40)[0]
    scene_size = struct.unpack_from("<Q", raw, scene_offset+8)[0]
    scene = json.loads(raw[scene_offset+24:scene_offset+24+scene_size])
    models = {int(model["id"]): model for model in scene["models"]}
    board_half = models[2]["box_size"][0] * models[2].get("scale", [1,1,1])[0] / 2
    block_half = models[3]["box_size"][0] * models[3].get("scale", [1,1,1])[0] / 2
    rows, separation, stop = [], None, None
    for time, bodies in read_frames(path):
        board, block = bodies[2], bodies[3]
        for body in (board, block):
            assert body[1] == body[8] == 0, "出现侧向漂移"
            assert body[10:13] == (0, 0, 0), "禁止转动失效"
        left = board[0] - board_half
        rows.append((time, left, board[7], block[0], block[7], left - block[0]))
        if separation is None and left > block[0] + block_half:
            separation = {"time_s": time, "block_speed_m_s": block[7]}
        if separation and stop is None and block[7] <= 1e-6:
            stop = {"time_s": time, "distance_m": left - block[0]}
            # 使用停止前两个实测速度估计减速度，消除整毫秒采样取整的距离误差。
            if len(rows) >= 3:
                previous, before = rows[-2], rows[-3]
                interval = previous[0]-before[0]
                deceleration = (previous[4]-before[4])/interval
                if deceleration < 0 and previous[4] > 0:
                    delta = -previous[4]/deceleration
                    if 0 <= delta <= time-previous[0]:
                        board_acceleration = (previous[2]-before[2])/interval
                        board_left = previous[1]+previous[2]*delta+0.5*board_acceleration*delta**2
                        block_x = previous[3]+previous[4]*delta+0.5*deceleration*delta**2
                        stop["interpolated_time_s"] = previous[0]+delta
                        stop["interpolated_distance_m"] = board_left-block_x
    assert separation and stop, "未检测到分离或停止事件"
    mid = min(rows, key=lambda row: abs(row[0] - 0.5))
    acceleration = mid[2] / mid[0]
    summary = {"source": str(path), "board_acceleration_m_s2": acceleration,
               "separation": separation, "stop": stop,
               "theory": {"board_acceleration_m_s2": 5, "separation_time_s": 1,
                          "separation_speed_m_s": 3, "stop_time_s": 2.5, "distance_m": 12.75},
               "distance_relative_error": (stop["distance_m"] - 12.75) / 12.75,
               "sampling_uncertainty_s": rows[1][0] - rows[0][0]}
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    assert abs(acceleration - 5) < 0.001, "加速度误差过大"
    assert abs(separation["block_speed_m_s"] - 3) < 0.01, "分离速度误差过大"
    assert abs(stop["time_s"] - 2.5) < 0.005, "停止时刻误差过大"
    assert abs(stop["distance_m"] - 12.75) < 0.04, "距离超出薄板近似误差预算"
    with path.with_suffix(".csv").open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.writer(stream)
        writer.writerow(("time_s", "board_left_m", "board_vx_m_s", "block_x_m",
                         "block_vx_m_s", "distance_m"))
        writer.writerows(rows)
    path.with_suffix(".json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="检查滑块木板题的仿真结果")
    parser.add_argument("file", type=Path)
    analyze(parser.parse_args().file)
