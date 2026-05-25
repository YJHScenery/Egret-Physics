import math

def generate_torus(major_radius, minor_radius, output_filename, radial_segments=100, tubular_segments=50):
    """
    生成环面OBJ模型

    参数:
        major_radius: 主半径（环面中心到管道中心距离）
        minor_radius: 管半径（环切面宽度的一半，即0.005）
        output_filename: 输出文件名
        radial_segments: 绕大圆的分段数（经向，默认100）
        tubular_segments: 绕小圆的分段数（纬向，默认50）
    """
    # 验证参数
    if minor_radius <= 0:
        raise ValueError("管半径必须大于0")
    if major_radius <= minor_radius:
        print(f"警告: 主半径 {major_radius} <= 管半径 {minor_radius}，环面会自交")

    vertices = []
    normals = []
    faces = []

    # 生成顶点和法线
    for i in range(radial_segments):
        u = 2 * math.pi * i / radial_segments  # 大圆角度

        for j in range(tubular_segments):
            v = 2 * math.pi * j / tubular_segments  # 小圆角度

            # 环面参数方程
            # x = (R + r * cos(v)) * cos(u)
            # y = (R + r * cos(v)) * sin(u)
            # z = r * sin(v)
            x = (major_radius + minor_radius * math.cos(v)) * math.cos(u)
            y = (major_radius + minor_radius * math.cos(v)) * math.sin(u)
            z = minor_radius * math.sin(v)

            # 法线计算（向外方向）
            # 法线 = (P - C) / |P - C|，其中C是环面中心线上的点
            # 简化计算：法线方向为从环面中心线指向表面点的方向
            center_x = major_radius * math.cos(u)
            center_y = major_radius * math.sin(u)
            center_z = 0

            nx = x - center_x
            ny = y - center_y
            nz = z - center_z

            # 归一化
            length = math.sqrt(nx*nx + ny*ny + nz*nz)
            if length > 0:
                nx /= length
                ny /= length
                nz /= length

            vertices.append((x, y, z))
            normals.append((nx, ny, nz))

    # 生成面（三角形面，索引从1开始）
    for i in range(radial_segments):
        for j in range(tubular_segments):
            i_next = (i + 1) % radial_segments
            j_next = (j + 1) % tubular_segments

            idx = i * tubular_segments + j
            idx_i_next_j = i_next * tubular_segments + j
            idx_i_next_j_next = i_next * tubular_segments + j_next
            idx_j_next = i * tubular_segments + j_next

            # 三角形1
            faces.append((idx + 1, idx_i_next_j + 1, idx_j_next + 1))
            # 三角形2
            faces.append((idx_i_next_j + 1, idx_i_next_j_next + 1, idx_j_next + 1))

    # 写入OBJ文件
    with open(output_filename, 'w') as f:
        f.write(f"# Torus model\n")
        f.write(f"# Major radius: {major_radius}\n")
        f.write(f"# Minor radius: {minor_radius}\n")
        f.write(f"# Radial segments: {radial_segments}\n")
        f.write(f"# Tubular segments: {tubular_segments}\n")
        f.write("\n")

        # 写入顶点
        for v in vertices:
            f.write(f"v {v[0]:.10f} {v[1]:.10f} {v[2]:.10f}\n")

        f.write("\n")

        # 写入法线
        for n in normals:
            f.write(f"vn {n[0]:.10f} {n[1]:.10f} {n[2]:.10f}\n")

        f.write("\n")

        # 写入面（引用顶点和法线）
        for face in faces:
            f.write(f"f {face[0]}//{face[0]} {face[1]}//{face[1]} {face[2]}//{face[2]}\n")

    print(f"已生成: {output_filename}")
    print(f"  顶点数: {len(vertices)}")
    print(f"  面数: {len(faces)}")
    print(f"  法线数: {len(normals)}")

def main():
    # 环切面宽度 = 0.01，所以管半径 = 宽度/2 = 0.005
    minor_radius = 0.005

    # 主半径列表
    major_radii = [0.1, 1, 10, 100, 1000, 10000]

    # 根据主半径调整分段数以控制文件大小
    def get_segments(radius):
        """根据半径大小调整分段数，保证视觉效果同时控制文件大小"""
        if radius <= 1:
            return 100, 50
        elif radius <= 10:
            return 100, 50
        elif radius <= 100:
            return 120, 60
        elif radius <= 1000:
            return 160, 80
        else:
            # 半径10000时，保持合理分段数，避免文件过大
            return 200, 100

    for major_radius in major_radii:
        # 检查自交警告
        if major_radius <= minor_radius:
            print(f"\n警告: 主半径 {major_radius} 小于等于管半径 {minor_radius}，几何体自交")

        # 获取分段数
        radial_seg, tubular_seg = get_segments(major_radius)

        # 生成文件名
        if major_radius == int(major_radius):
            filename = f"torus_R{major_radius:.0f}.obj"
        else:
            filename = f"torus_R{major_radius}.obj"

        # 生成模型
        generate_torus(major_radius, minor_radius, filename, radial_seg, tubular_seg)

    print("\n所有模型生成完成！")

if __name__ == "__main__":
    main()