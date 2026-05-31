import numpy as np

def generate_spiral_obj(filename, turns, height, t_start=0, t_end=1, num_points=500):
    """
    生成等距等宽螺旋线的OBJ文件

    参数:
        filename: 输出OBJ文件名
        turns: 螺旋线匝数（圈数）
        height: 螺旋线总高度（z从0到height）
        t_start: 起始t值
        t_end: 结束t值
        num_points: 离散点数量
    """
    # 根据匝数计算a参数：a = 2π × turns
    a = 2 * np.pi * turns

    # 生成参数t序列
    t_values = np.linspace(t_start, t_end, num_points)

    # 计算坐标点
    x = np.cos(a * t_values)
    y = np.sin(a * t_values)
    z = height * t_values

    # 打开文件写入
    with open(filename, 'w') as f:
        # 写入OBJ头注释
        f.write(f"# 等距等宽螺旋线\n")
        f.write(f"# 参数: 匝数={turns}, 高度={height}\n")
        f.write(f"# t范围: [{t_start}, {t_end}], 点数: {num_points}\n")
        f.write(f"# 方程: z = height * t, x = cos(2π*turns*t), y = sin(2π*turns*t)\n\n")

        # 写入顶点
        for i in range(num_points):
            f.write(f"v {x[i]} {y[i]} {z[i]}\n")

        f.write("\n")

        # 写入线段（使用l表示直线）
        for i in range(1, num_points):
            f.write(f"l {i} {i+1}\n")

        # 可选：如果想创建闭合环，取消下面注释
        # f.write(f"l {num_points} 1\n")

    print(f"OBJ文件已生成: {filename}")
    print(f"匝数: {turns}, 高度: {height}")
    print(f"顶点数: {num_points}")
    print(f"线段数: {num_points-1}")

def generate_spiral_with_tube(filename, turns, height, t_start=0, t_end=1,
                              num_points=200, tube_radius=0.02, tube_segments=8):
    """
    生成带管状厚度的螺旋线OBJ文件（多边形网格）

    参数:
        filename: 输出OBJ文件名
        turns: 螺旋线匝数（圈数）
        height: 螺旋线总高度
        t_start: 起始t值
        t_end: 结束t值
        num_points: 沿螺旋线的分段数
        tube_radius: 管半径
        tube_segments: 每个截面的细分段数（圆的多边形边数）
    """
    # 根据匝数计算a参数
    a = 2 * np.pi * turns

    # 生成t值
    t_values = np.linspace(t_start, t_end, num_points)

    # 计算螺旋线上的点
    points = np.zeros((num_points, 3))
    points[:, 0] = np.cos(a * t_values)
    points[:, 1] = np.sin(a * t_values)
    points[:, 2] = height * t_values

    # 计算切线和局部坐标系
    tangents = np.zeros((num_points, 3))
    tangents[0] = points[1] - points[0]
    for i in range(1, num_points-1):
        tangents[i] = points[i+1] - points[i-1]
    tangents[-1] = points[-1] - points[-2]

    # 归一化切线
    for i in range(num_points):
        norm = np.linalg.norm(tangents[i])
        if norm > 0:
            tangents[i] /= norm
        else:
            tangents[i] = np.array([0, 0, 1])

    # 选择参考向量构建局部坐标系
    up = np.array([0, 0, 1])

    # 存储每个点的圆环顶点
    circle_vertices = []

    # 先计算所有点处的圆环顶点
    for i in range(num_points):
        tangent = tangents[i]

        # 计算局部x轴：垂直于切线和全局上向量
        if np.abs(np.dot(tangent, up)) > 0.9999:  # 切线接近平行于z轴
            local_x = np.array([1, 0, 0])
        else:
            local_x = np.cross(up, tangent)
            local_x = local_x / np.linalg.norm(local_x)

        # 计算局部y轴：垂直于切线和局部x轴
        local_y = np.cross(tangent, local_x)
        local_y = local_y / np.linalg.norm(local_y)

        # 生成当前点处的圆环顶点
        circle = []
        for j in range(tube_segments):
            angle = 2 * np.pi * j / tube_segments
            offset = tube_radius * (np.cos(angle) * local_x + np.sin(angle) * local_y)
            vertex = points[i] + offset
            circle.append(vertex)
        circle_vertices.append(circle)

    # 写入OBJ文件
    with open(filename, 'w') as f:
        f.write(f"# 等距等宽螺旋线（管状网格）\n")
        f.write(f"# 参数: 匝数={turns}, 高度={height}\n")
        f.write(f"# 管半径: {tube_radius}, 截面分段: {tube_segments}, 轴向分段: {num_points}\n\n")

        # 写入所有顶点
        for i in range(num_points):
            for j in range(tube_segments):
                v = circle_vertices[i][j]
                f.write(f"v {v[0]} {v[1]} {v[2]}\n")

        f.write("\n")

        # 写入面
        def vertex_index(i, j):
            return i * tube_segments + j + 1

        for i in range(num_points - 1):
            for j in range(tube_segments):
                j_next = (j + 1) % tube_segments
                f.write(f"f {vertex_index(i, j)} {vertex_index(i, j_next)} "
                        f"{vertex_index(i+1, j_next)} {vertex_index(i+1, j)}\n")

    print(f"管状网格OBJ文件已生成: {filename}")
    print(f"匝数: {turns}, 高度: {height}")
    print(f"顶点数: {num_points * tube_segments}")
    print(f"面数: {(num_points-1) * tube_segments}")

def generate_spiral_cylinder_surface(filename, turns, height, radius=1.0,
                                     num_points=100, num_rings=20):
    """
    生成螺旋线圆柱面（螺旋带）的OBJ文件

    参数:
        filename: 输出OBJ文件名
        turns: 螺旋线匝数
        height: 总高度
        radius: 圆柱半径
        num_points: 沿螺旋线的分段数（长度方向）
        num_rings: 径向分段数（从中心到外缘的分段）
    """
    a = 2 * np.pi * turns
    t_values = np.linspace(0, 1, num_points)
    r_values = np.linspace(0, radius, num_rings)

    vertices = []
    faces = []

    # 生成顶点：对于每个t和每个r
    for i, t in enumerate(t_values):
        angle = a * t
        base_x = np.cos(angle)
        base_y = np.sin(angle)
        base_z = height * t

        for j, r in enumerate(r_values):
            # 径向缩放的顶点
            x = r * base_x
            y = r * base_y
            z = base_z
            vertices.append((x, y, z))

    # 生成面（四边形）
    for i in range(num_points - 1):
        for j in range(num_rings - 1):
            # 四个顶点的索引
            v00 = i * num_rings + j + 1
            v01 = i * num_rings + (j + 1) + 1
            v10 = (i + 1) * num_rings + j + 1
            v11 = (i + 1) * num_rings + (j + 1) + 1
            faces.append((v00, v01, v11, v10))

    # 写入文件
    with open(filename, 'w') as f:
        f.write(f"# 螺旋线圆柱面\n")
        f.write(f"# 匝数={turns}, 高度={height}, 半径={radius}\n\n")

        for v in vertices:
            f.write(f"v {v[0]} {v[1]} {v[2]}\n")

        f.write("\n")
        for face in faces:
            f.write(f"f {face[0]} {face[1]} {face[2]} {face[3]}\n")

    print(f"螺旋圆柱面OBJ文件已生成: {filename}")
    print(f"匝数: {turns}, 高度: {height}, 半径: {radius}")
    print(f"顶点数: {len(vertices)}")
    print(f"面数: {len(faces)}")

# 示例使用
if __name__ == "__main__":
    print("=" * 50)
    print("生成不同匝数的螺旋线")
    print("=" * 50)

    # 示例1：1圈螺旋，高度1.0
    print("\n1. 生成1圈螺旋线")
    generate_spiral_obj("spiral_1turn.obj", turns=1, height=1.0, num_points=300)

    # 示例2：2圈螺旋，高度1.5
    print("\n2. 生成2圈螺旋线")
    generate_spiral_obj("spiral_2turns.obj", turns=2, height=1.5, num_points=400)

    # 示例3：3圈螺旋，高度2.0
    print("\n3. 生成3圈螺旋线")
    generate_spiral_obj("spiral_3turns.obj", turns=3, height=2.0, num_points=500)

    # 示例4：5圈螺旋，高度2.5（密绕）
    print("\n4. 生成5圈螺旋线（密绕）")
    generate_spiral_obj("spiral_5turns.obj", turns=5, height=2.5, num_points=800)

    # 示例5：管状螺旋（1.5圈）
    print("\n5. 生成管状螺旋网格（1.5圈）")
    generate_spiral_with_tube("spiral_tube_1.5turns.obj", turns=32, height=100,
                              num_points=300, tube_radius=0.04, tube_segments=1024)

    # 示例6：螺旋圆柱面（2圈）
    print("\n6. 生成螺旋圆柱面（2圈）")
    generate_spiral_cylinder_surface("spiral_surface_2turns.obj", turns=2, height=1.5,
                                     radius=1.0, num_points=80, num_rings=10)

    print("\n" + "=" * 50)
    print("生成的文件说明:")
    print("=" * 50)
    print("- spiral_1turn.obj: 1圈螺旋线")
    print("- spiral_2turns.obj: 2圈螺旋线")
    print("- spiral_3turns.obj: 3圈螺旋线")
    print("- spiral_5turns.obj: 5圈螺旋线")
    print("- spiral_tube_1.5turns.obj: 管状螺旋（1.5圈）")
    print("- spiral_surface_2turns.obj: 螺旋圆柱面（2圈）")