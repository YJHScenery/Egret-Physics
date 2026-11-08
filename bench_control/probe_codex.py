"""@file probe_codex.py
@brief 从 Codex 会话记录中提取与 epscenex / 受限 C / Lua 控制注入相关的对话片段。
"""

import json
import pathlib
import re
import sys

KEYWORDS = re.compile(r"epscenex|受限\s*Lua|backend|programs|场景包|控制注入")


def iter_texts(path):
    """@brief 逐行解析 jsonl，产出 (角色, 文本) 对。"""
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                continue
            payload = record.get("payload")
            if not isinstance(payload, dict):
                continue
            if payload.get("type") != "message":
                continue
            role = payload.get("role", "?")
            parts = []
            for item in payload.get("content", []):
                if isinstance(item, dict) and item.get("text"):
                    parts.append(item["text"])
            if parts:
                yield role, "\n".join(parts)


def main():
    """@brief 打印命中的片段。"""
    for raw in sys.argv[1:]:
        path = pathlib.Path(raw)
        print("=" * 20, path.name)
        count = 0
        for role, text in iter_texts(path):
            if not KEYWORDS.search(text):
                continue
            for match in KEYWORDS.finditer(text):
                start = max(0, match.start() - 300)
                end = min(len(text), match.end() + 500)
                print("[%s] ...%s..." % (role, text[start:end].replace("\n", " ")))
                count += 1
                if count >= 12:
                    return
        print("hits:", count)


if __name__ == "__main__":
    main()
