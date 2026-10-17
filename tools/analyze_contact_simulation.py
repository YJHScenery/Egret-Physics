"""@file analyze_contact_simulation.py
@brief 只读提取 EPSX 刚体尾段振幅与速度，支持修复前后量化对比。
"""
import argparse
import json
import math
import struct
from pathlib import Path


def build_crc_table():
    """@brief 构造 CRC-32C 的反射多项式查找表。"""
    table = []
    for index in range(256):
        value = index
        for bit in range(8):
            value = (value >> 1) ^ (0x82F63B78 if value & 1 else 0)
        table.append(value)
    return table


CRC_TABLE = build_crc_table()


def crc32c(data):
    """@brief 验证文件头和块载荷的 CRC-32C 校验值。"""
    value = 0xFFFFFFFF
    for byte in data:
        value = CRC_TABLE[(value ^ byte) & 0xFF] ^ (value >> 8)
    return value ^ 0xFFFFFFFF


def analyze(path):
    """@brief 按文件定义读取帧段，统计最后五秒的高度、速度与角速度。"""
    bodies = {}
    metadata = None
    frame_count = 0
    with path.open("rb") as stream:
        header = stream.read(64)
        if header[:8] != b"\x89EPSX\r\n\x1a":
            raise ValueError("EPSX 魔数不匹配")
        if crc32c(header[:60]) != struct.unpack_from("<I", header, 60)[0]:
            raise ValueError("文件头 CRC 校验失败")
        major, minor, flags = struct.unpack_from("<HHI", header, 8)
        if major != 1 or flags != 1:
            raise ValueError("仅支持未压缩 float64 的 EPSX v1")
        expected_frames = struct.unpack_from("<Q", header, 16)[0]
        interval = struct.unpack_from("<I", header, 32)[0] / 1e9
        tail_start = max(0.0, (expected_frames - 1) * interval - 5.0)
        while chunk := stream.read(24):
            if len(chunk) != 24:
                raise ValueError("块头被截断")
            tag, chunk_flags, length, crc, version = struct.unpack("<4sIQII", chunk)
            payload = stream.read(length)
            if len(payload) != length:
                raise ValueError("块载荷被截断")
            if crc32c(payload) != crc:
                raise ValueError(f"块 {tag!r} CRC 校验失败")
            stream.seek((-length) % 8, 1)
            if tag == b"SCNE":
                metadata = json.loads(payload)
            elif tag == b"FRAM":
                frame_index, frame_flags, time_ns, step = struct.unpack_from("<IIQQ", payload)
                frame_count += 1
                if time_ns / 1e9 < tail_start:
                    continue
                offset = 24
                while offset + 32 <= len(payload):
                    section, count, stride, section_flags, size, section_crc, section_version = struct.unpack_from("<4sIIIQII", payload, offset)
                    offset += 32
                    if offset + size > len(payload):
                        raise ValueError("帧段越界")
                    if section == b"BODY":
                        if stride != 144:
                            raise ValueError("不支持的刚体记录步长")
                        for index in range(count):
                            record = offset + index * stride
                            identity, slot, state = struct.unpack_from("<QII", payload, record)
                            values = struct.unpack_from("<15d", payload, record + 16)
                            position = values[:3]
                            velocity = values[7:10]
                            angular = values[10:13]
                            if not all(math.isfinite(value) for value in values):
                                raise ValueError("刚体记录含非有限值")
                            item = bodies.setdefault(identity, {"z_min": math.inf, "z_max": -math.inf,
                                "v2_sum": 0.0, "w2_sum": 0.0, "speed_max": 0.0, "count": 0,
                                "asleep_samples": 0})
                            item["z_min"] = min(item["z_min"], position[2])
                            item["z_max"] = max(item["z_max"], position[2])
                            speed2 = sum(value * value for value in velocity)
                            item["v2_sum"] += speed2
                            item["w2_sum"] += sum(value * value for value in angular)
                            item["speed_max"] = max(item["speed_max"], math.sqrt(speed2))
                            item["count"] += 1
                            item["asleep_samples"] += int(bool(state & 1))
                            item["last_position"] = position
                            item["last_velocity"] = velocity
                            item["last_angular"] = angular
                    offset += size + (-size) % 8
    for item in bodies.values():
        item["z_amplitude"] = item["z_max"] - item["z_min"]
        item["speed_rms"] = math.sqrt(item.pop("v2_sum") / item["count"])
        item["angular_rms"] = math.sqrt(item.pop("w2_sum") / item["count"])
    return {"file": str(path.resolve()), "frames": frame_count, "sample_interval": interval,
            "tail_start": tail_start, "sampling": metadata.get("sampling", {}) if metadata else {},
            "solver": metadata.get("solver", {}) if metadata else {}, "bodies": bodies,
            "validation": "已检查结构、有限值、文件头和全部块 CRC；采样曲线不能排除帧间高频振荡"}


def main():
    """@brief 输出 JSON 统计，原始仿真文件保持只读。"""
    parser = argparse.ArgumentParser(description="统计 EPSX 尾段接触稳定性")
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", type=Path)
    arguments = parser.parse_args()
    report = json.dumps(analyze(arguments.input), ensure_ascii=False, indent=2)
    if arguments.output:
        arguments.output.write_text(report + "\n", encoding="utf-8")
    else:
        print(report)


if __name__ == "__main__":
    main()
