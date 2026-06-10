import numpy as np

def generate_sphere_obj(radius=1.0, slices=32, stacks=16, filename="sphere.obj"):
    """
    生成半径为指定值的球体的OBJ文件

    参数:
        radius: 球体半径，默认为1.0
        slices: 经线分割数（围绕Y轴的细分），默认为32
        stacks: 纬线分割数（从北极到南极的细分），默认为16
        filename: 输出的OBJ文件名，默认为"sphere.obj"
    """
    vertices = []
    faces = []
    normals = []

    # 生成顶点和法线
    for i in range(stacks + 1):
        # theta: 极角（从北极到南极），范围 0 到 pi
        theta = i * np.pi / stacks
        sin_theta = np.sin(theta)
        cos_theta = np.cos(theta)

        for j in range(slices + 1):
            # phi: 方位角（环绕Y轴），范围 0 到 2*pi
            phi = j * 2 * np.pi / slices
            sin_phi = np.sin(phi)
            cos_phi = np.cos(phi)

            # 顶点坐标
            x = radius * sin_theta * cos_phi
            y = radius * cos_theta
            z = radius * sin_theta * sin_phi

            vertices.append((x, y, z))

            # 法线（对于球体，法线与顶点方向相同）
            norm = (x / radius, y / radius, z / radius)
            normals.append(norm)

    # 生成面（三角形）
    for i in range(stacks):
        for j in range(slices):
            # 四个顶点索引
            p1 = i * (slices + 1) + j
            p2 = i * (slices + 1) + j + 1
            p3 = (i + 1) * (slices + 1) + j
            p4 = (i + 1) * (slices + 1) + j + 1

            # OBJ索引从1开始，所以+1
            # 两个三角形组成一个矩形面
            # 三角形1: p1, p2, p3
            faces.append((p1 + 1, p2 + 1, p3 + 1))
            # 三角形2: p2, p4, p3
            faces.append((p2 + 1, p4 + 1, p3 + 1))

    # 写入OBJ文件
    with open(filename, 'w') as f:
        f.write(f"# 半径为 {radius} 的球体 OBJ 文件\n")
        f.write(f"# 经线分割数: {slices}, 纬线分割数: {stacks}\n")
        f.write(f"# 顶点总数: {len(vertices)}, 面总数: {len(faces)}\n\n")

        # 写入顶点
        for v in vertices:
            f.write(f"v {v[0]} {v[1]} {v[2]}\n")

        f.write("\n")

        # 写入法线
        for n in normals:
            f.write(f"vn {n[0]} {n[1]} {n[2]}\n")

        f.write("\n")

        # 写入面（包含顶点和法线索引）
        for face in faces:
            # 格式: f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3
            # 此处没有纹理坐标，所以是 v1//vn1
            f.write(f"f {face[0]}//{face[0]} {face[1]}//{face[1]} {face[2]}//{face[2]}\n")

    print(f"球体OBJ文件已生成: {filename}")
    print(f"半径: {radius}, 顶点数: {len(vertices)}, 面数: {len(faces)}")

if __name__ == "__main__":
    # 生成半径为1的球体，默认经线32，纬线16
    generate_sphere_obj(radius=100.0, slices=32, stacks=16)