import math

def generate_normalized_cylinder_obj(filepath, radius=1.0, height=1.0, radial_segments=36):
    """
    生成一个归一化圆柱侧面的OBJ模型文件。

    参数:
        filepath (str): 输出OBJ文件的路径。
        radius (float): 圆柱的半径，默认为1.0。
        height (float): 圆柱的高度，默认为1.0。
        radial_segments (int): 圆柱侧面的细分段数（围绕圆周的分段数），默认为36。
    """
    vertices = []
    texcoords = []
    faces = []

    # 生成顶点和纹理坐标
    for i in range(radial_segments + 1):
        # 角度从0到2*PI
        angle = 2 * math.pi * i / radial_segments
        x = radius * math.cos(angle)
        z = radius * math.sin(angle)

        # 底部顶点 (y = -height/2)
        vertices.append((x, -height / 2, z))
        # 顶部顶点 (y = height/2)
        vertices.append((x, height / 2, z))

        # 纹理坐标: u 沿圆周方向 (0~1), v 沿高度方向 (0底部,1顶部)
        u = i / radial_segments
        texcoords.append((u, 0))  # 底部纹理坐标
        texcoords.append((u, 1))  # 顶部纹理坐标

    # 生成面（四边形，每个四边形由两个三角形组成）
    for i in range(radial_segments):
        # 获取四个顶点的索引
        # 第i段底部顶点索引
        idx_bottom_left = 2 * i
        # 第i段顶部顶点索引
        idx_top_left = 2 * i + 1
        # 下一段底部顶点索引（环形连接）
        idx_bottom_right = 2 * ((i + 1) % radial_segments)
        # 下一段顶部顶点索引
        idx_top_right = 2 * ((i + 1) % radial_segments) + 1

        # OBJ顶点索引从1开始
        v1 = idx_bottom_left + 1
        v2 = idx_top_left + 1
        v3 = idx_top_right + 1
        v4 = idx_bottom_right + 1

        # 使用两个三角形组成四边形：三角形1 (v1, v2, v3) 和 三角形2 (v1, v3, v4)
        # 格式: f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3 (如果使用纹理坐标)
        # 这里我们只输出顶点索引，不带纹理和法线，保持简洁。也可以输出带纹理坐标的格式。
        # 为了便于纹理映射，我们输出带纹理坐标的面。
        # 注意：纹理坐标索引与顶点索引一一对应（每个顶点对应一个纹理坐标）
        # 但因为我们没有共享纹理坐标不同的相同位置顶点，所以可以直接用同一索引。

        # 第一个三角形
        faces.append((v1, v2, v3))
        # 第二个三角形
        faces.append((v1, v3, v4))

    # 写入OBJ文件
    with open(filepath, 'w') as f:
        f.write("# Normalized Cylinder (Side Surface Only)\n")
        f.write(f"# Radius: {radius}, Height: {height}, Radial Segments: {radial_segments}\n")
        f.write("o Cylinder_Side\n")

        # 写入顶点
        for v in vertices:
            f.write(f"v {v[0]} {v[1]} {v[2]}\n")

        # 写入纹理坐标 (vt)
        for vt in texcoords:
            f.write(f"vt {vt[0]} {vt[1]}\n")

        # 写入面（使用顶点和纹理坐标索引）
        for face in faces:
            # OBJ面格式: f v1/vt1 v2/vt2 v3/vt3
            f.write(f"f {face[0]}/{face[0]} {face[1]}/{face[1]} {face[2]}/{face[2]}\n")

    print(f"OBJ file generated: {filepath}")
    print(f"Vertices: {len(vertices)}, Faces: {len(faces)}")

if __name__ == "__main__":
    # 示例：生成一个半径1，高度1，细分36段的圆柱侧面OBJ文件
    generate_normalized_cylinder_obj("../resources/assets/model_3d/cylinder_side/cylinder_side.obj", radius=1.0, height=1.0, radial_segments=128)