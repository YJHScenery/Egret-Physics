import math

def create_double_sided_circle_obj(filename, radius=0.5, segments=128):
    """
    创建双面圆形OBJ文件（正反两面都有三角形）
    """
    vertices = []
    faces = []

    # 顶层顶点（Z=0）
    vertices.append((0.0, 0.0, 0.0))  # 顶层圆心

    for i in range(segments):
        angle = 2 * math.pi * i / segments
        x = radius * math.cos(angle)
        y = radius * math.sin(angle)
        vertices.append((x, y, 0.0))  # 顶层圆周点

    # 底层顶点（Z=0，位置相同但独立顶点）
    bottom_center_idx = len(vertices)
    vertices.append((0.0, 0.0, 0.0))  # 底层圆心

    for i in range(segments):
        angle = 2 * math.pi * i / segments
        x = radius * math.cos(angle)
        y = radius * math.sin(angle)
        vertices.append((x, y, 0.0))  # 底层圆周点

    # 生成正面三角形（法线朝上）
    for i in range(segments):
        v1 = 1  # 顶层圆心
        v2 = i + 2  # 当前圆周点
        v3 = i + 3 if i < segments - 1 else 2  # 下一个圆周点
        faces.append((v1, v2, v3, "top"))

    # 生成背面三角形（法线朝下，顶点顺序相反）
    offset = bottom_center_idx + 1
    for i in range(segments):
        v1 = bottom_center_idx + 1  # 底层圆心
        v2 = offset + i  # 当前圆周点
        v3 = offset + (i + 1 if i < segments - 1 else 0)  # 下一个圆周点
        # 注意：背面使用相反的顶点顺序使法线朝下
        faces.append((v1, v3, v2, "bottom"))

    # 写入文件
    with open(filename, 'w') as f:
        f.write("# 双面圆形模型\n")
        f.write(f"# 直径: {radius*2}, 分片: {segments}\n")
        f.write("o DoubleSidedCircle\n\n")

        # 写入所有顶点
        for v in vertices:
            f.write(f"v {v[0]:.6f} {v[1]:.6f} {v[2]:.6f}\n")

        f.write("\n# 正面 (法线朝上)\n")
        for face in faces:
            if face[3] == "top":
                f.write(f"f {face[0]} {face[1]} {face[2]}\n")

        f.write("\n# 背面 (法线朝下)\n")
        for face in faces:
            if face[3] == "bottom":
                f.write(f"f {face[0]} {face[1]} {face[2]}\n")

    print(f"双面圆形文件已创建: {filename}")
    print(f"分段数: {segments}")
    print(f"总顶点数: {len(vertices)}")
    print(f"总面数: {len(faces)}")

# 创建双面圆形
create_double_sided_circle_obj("circle_double_sided.obj", radius=0.5, segments=128)