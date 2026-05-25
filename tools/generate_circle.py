import math

def generate_normalized_circle_obj(filepath, radius=1.0, segments=128):
    """
    生成一个归一化圆的OBJ模型文件（位于XZ平面上的圆形面片）。

    参数:
        filepath (str): 输出OBJ文件的路径。
        radius (float): 圆的半径，默认为1.0。
        segments (int): 圆的细分段数（圆周上的顶点数），默认为128。
    """
    vertices = []
    texcoords = []
    faces = []

    # 添加中心顶点 (索引0)
    vertices.append((0.0, 0.0, 0.0))
    texcoords.append((0.5, 0.5))  # 中心纹理坐标

    # 生成圆周上的顶点
    for i in range(segments):
        angle = 2 * math.pi * i / segments
        x = radius * math.cos(angle)
        z = radius * math.sin(angle)
        vertices.append((x, 0.0, z))

        # 纹理坐标: u从0到1映射角度，v从0到1映射半径
        # 这里使用等面积映射方式：u = (cos(angle)+1)/2, v = (sin(angle)+1)/2
        # 但为了简单且不失一般性，使用极坐标映射：u = 0.5 + 0.5*cos(angle), v = 0.5 + 0.5*sin(angle)
        u = 0.5 + 0.5 * math.cos(angle)
        v = 0.5 + 0.5 * math.sin(angle)
        texcoords.append((u, v))

    # 生成三角形面（中心与相邻两个圆周顶点）
    for i in range(segments):
        # 顶点索引（OBJ从1开始，中心为1，圆周顶点从2到segments+1）
        center_idx = 1
        curr_idx = i + 2  # 当前圆周顶点索引
        next_idx = ((i + 1) % segments) + 2  # 下一个圆周顶点索引

        # 三角形面：中心、当前顶点、下一个顶点
        faces.append((center_idx, curr_idx, next_idx))

    # 写入OBJ文件
    with open(filepath, 'w') as f:
        f.write("# Normalized Circle (XZ Plane)\n")
        f.write(f"# Radius: {radius}, Segments: {segments}\n")
        f.write("o Circle\n")

        # 写入顶点
        for v in vertices:
            f.write(f"v {v[0]} {v[1]} {v[2]}\n")

        # 写入纹理坐标
        for vt in texcoords:
            f.write(f"vt {vt[0]} {vt[1]}\n")

        # 写入面（使用顶点和纹理坐标索引）
        for face in faces:
            f.write(f"f {face[0]}/{face[0]} {face[1]}/{face[1]} {face[2]}/{face[2]}\n")

    print(f"OBJ file generated: {filepath}")
    print(f"Vertices: {len(vertices)}, Faces: {len(faces)}")

if __name__ == "__main__":
    # 生成半径1，分片128的圆OBJ文件
    generate_normalized_circle_obj("../resources/assets/model_3d/circle_disk/circle.obj", radius=1.0, segments=128)