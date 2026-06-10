import math

def generate_icosahedron_obj(radius=50.0, filename="icosahedron.obj"):
    """
    通过几何构造生成正二十面体（不使用预定义列表）
    原理：三个相互正交的黄金矩形顶点构成正二十面体的12个顶点
    """
    phi = (1.0 + math.sqrt(5.0)) / 2.0  # 黄金比例

    # 步骤1：计算归一化顶点（外接球半径为1时）
    # 三个黄金矩形的顶点坐标：组合 (±1, ±φ, 0), (0, ±1, ±φ), (±φ, 0, ±1)
    coords = [
        # 矩形1： (±1, ±φ, 0)
        ( 1,  phi, 0), ( 1, -phi, 0), (-1,  phi, 0), (-1, -phi, 0),
        # 矩形2： (0, ±1, ±φ)
        ( 0,  1,  phi), ( 0,  1, -phi), ( 0, -1,  phi), ( 0, -1, -phi),
        # 矩形3： (±φ, 0, ±1)
        ( phi, 0,  1), ( phi, 0, -1), (-phi, 0,  1), (-phi, 0, -1)
    ]

    # 步骤2：缩放到指定半径
    norm_radius = math.sqrt(1 + phi * phi)
    scale = radius / norm_radius
    vertices = [(x * scale, y * scale, z * scale) for (x, y, z) in coords]

    # 步骤3：通过距离计算找出每个顶点的5个邻居（构成20个三角形面）
    # 原理：正二十面体中，每个顶点距离最近的5个顶点与其相连
    def distance_sq(v1, v2):
        return (v1[0]-v2[0])**2 + (v1[1]-v2[1])**2 + (v1[2]-v2[2])**2

    # 计算所有顶点对的距离平方
    n = len(vertices)
    edges = []
    for i in range(n):
        dists = []
        for j in range(n):
            if i != j:
                dists.append((distance_sq(vertices[i], vertices[j]), j))
        dists.sort()
        # 最近的5个顶点就是邻居（正二十面体每个顶点有5个邻居）
        neighbors = [j for _, j in dists[:5]]
        # 记录边（去重）
        for nb in neighbors:
            if i < nb:
                edges.append((i, nb))

    # 步骤4：从边构建三角形面
    # 将边转换为邻接表
    adj = [[] for _ in range(n)]
    for a, b in edges:
        adj[a].append(b)
        adj[b].append(a)

    # 对每个顶点，遍历其邻居对，找共同邻居形成三角形
    faces_set = set()
    for i in range(n):
        neighbors = adj[i]
        # 遍历所有邻居对
        for j in range(len(neighbors)):
            for k in range(j + 1, len(neighbors)):
                a, b = neighbors[j], neighbors[k]
                # 检查 a 和 b 是否相连
                if b in adj[a]:
                    # 对三角形顶点排序，以便去重
                    face = tuple(sorted([i, a, b]))
                    faces_set.add(face)

    faces = list(faces_set)

    # 步骤5：写入OBJ文件
    with open(filename, 'w') as f:
        f.write("# Regular icosahedron (geometrically constructed)\n")
        f.write(f"# Outer diameter: {radius * 2}\n")
        f.write(f"# Vertices: {n}, Faces: {len(faces)}\n\n")

        for v in vertices:
            f.write(f"v {v[0]:.6f} {v[1]:.6f} {v[2]:.6f}\n")

        f.write("\n")

        for face in faces:
            # 调整顶点顺序使法线向外
            v0, v1, v2 = face
            p0, p1, p2 = vertices[v0], vertices[v1], vertices[v2]
            # 计算中心到面的方向
            center = (0, 0, 0)
            face_center = ((p0[0]+p1[0]+p2[0])/3,
                           (p0[1]+p1[1]+p2[1])/3,
                           (p0[2]+p1[2]+p2[2])/3)
            # 计算法线
            e1 = (p1[0]-p0[0], p1[1]-p0[1], p1[2]-p0[2])
            e2 = (p2[0]-p0[0], p2[1]-p0[1], p2[2]-p0[2])
            normal = (e1[1]*e2[2] - e1[2]*e2[1],
                      e1[2]*e2[0] - e1[0]*e2[2],
                      e1[0]*e2[1] - e1[1]*e2[0])
            # 点积判断方向：如果法线指向外则保持，否则反转
            if normal[0]*face_center[0] + normal[1]*face_center[1] + normal[2]*face_center[2] < 0:
                # 反转顶点顺序
                f.write(f"f {v0+1} {v2+1} {v1+1}\n")
            else:
                f.write(f"f {v0+1} {v1+1} {v2+1}\n")

    print(f"已生成: {filename}")
    print(f"顶点数: {len(vertices)}")
    print(f"边数: {len(edges)}")
    print(f"面数: {len(faces)}")
    print(f"外径: {radius * 2}")


if __name__ == "__main__":
    generate_icosahedron_obj(50.0, "icosahedron.obj")