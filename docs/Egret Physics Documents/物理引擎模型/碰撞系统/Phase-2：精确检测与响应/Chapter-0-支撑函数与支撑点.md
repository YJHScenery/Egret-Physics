# 支撑函数

**支撑函数**（Support Function）是凸分析和凸几何中的一个核心概念，它用来描述一个集合在任意方向上的“最远支撑点”的距离。

设 $K \subset \mathbb{R}^n$ 为一个非空闭合凸集，则对于任意的向量 $y \in \mathbb{R}^n$，集合 $K$ 在方向 $y$ 上的支撑函数 $h_K(y)$ 定义为：
$$
h_K(\mathbf{y}) = \sup_{\mathbf{x}\in K}{\left ( \langle \mathbf{x} , \mathbf{y} \rangle \right) }
$$
其中 $\sup$ 表示上确界（或最大值），运算 $\langle \mathbf{u}, \mathbf{v} \rangle$ 表示求向量 $\mathbf{u}$ 和向量 $\mathbf{v}$ 的点积。

对于 **三维闭合凸体**，此函数可以以程序化的语言表达为：遍历凸体平面及棱上的所有点 $P$，原点 $O$ 与 $P$ 构成向量 $\mathbf{b} = \mathbf{OP}$，给定方向向量 $\mathbf{d}$，计算数量积 $\mathbf{d \cdot b}$ 的最大值。

## 支撑点

获得支撑函数所需上确界时的向量 $\mathbf{x}$ 即支撑点，当支撑点 $\mathbf{x}$ 存在且唯一时，定义支撑点函数 $\mathbf{x}^{*}(\mathbf{y})$ 为：
$$
\mathbf{x}^*(\mathbf{y}) = \nabla{h_K{\left( \mathbf{y} \right)}}
$$
若函数 $h_K(\mathbf{y})$ 在 $\mathbf{y} = \mathbf{y}_0$ 处梯度不存在，则支撑点函数 $\mathbf{x}^*{\mathbf{y}}$ 的结果为一个集合，其值为函数 $h_K(\mathbf{y})$ 的 **次微分**：
$$
\mathbf{x}^{*}(\mathbf{y}) = \partial h_K(\mathbf{y})
$$
### 次梯度与次微分

**定义** 设 $f: \mathbb{R}^n \to \mathbb{R}$ 是一个 **凸函数**，$\mathbf{x}_0 \in \mathbb{R}^n$。向量 $\mathbf{g} \in \mathbb{R}^n$ 称为 $f$ 在点 $\mathbf{x}_0$ 处的 **次梯度**，当且仅当对于所有的 $\mathbf{x} \in \mathbb{R}^n$，有：
$$
f(\mathbf{x}) \geq f(\mathbf{x}_0) + \langle \mathbf{g}, \mathbf{x - x_0}\rangle
$$
所有满足该条件的向量 $\mathbf{g}$ 组成的集合称为函数 $f$ 在点 $\mathbf{x}_0$ 处的 **次微分**，记作：$\partial f(\mathbf{x}_0)$。

**凸函数**：设 **连续** 函数 $f: \mathbb{R}^n \to \mathbb{R}$，$f$ 为 **凸函数** 当且仅当：
$$
\forall \mathbf{x}_1, \mathbf{x}_2 \in \mathbb{R}^n: \frac{f(\mathbf{x}_1)+f(\mathbf{x}_2)}{2} \leq f(\frac{\mathbf{x}_1 + \mathbf{x}_2}{2})
$$
或给出严格定义：
设 $f: \mathbb{R}^n \to \mathbb{R} \cup \{+\infty\}$ 为扩展实值函数，其有效定义域 $\operatorname{dom}(f) = \{ \mathbf{x} \in \mathbb{R}^n \mid f(\mathbf{x}) < +\infty \}$ 非空。若满足：
1. $\operatorname{dom}(f)$ 是凸集；
2. 对任意 $\mathbf{x}, \mathbf{y} \in \operatorname{dom}(f)$，任意 $\lambda \in [0,1]$，有
$$
f(\lambda \mathbf{x} + (1-\lambda)\mathbf{y}) \le \lambda f(\mathbf{x}) + (1-\lambda)f(\mathbf{y})
$$
则称 $f$ 为凸函数。

## 函数实现

本系统对下述的三维凸体模型实现支撑点函数：**长方盒体、圆柱体、二维圆盘、胶囊体、球体**。
在 GJK 算法中，对于支撑函数 $h_K(\mathbf{y})$ 不连续情形，只需要任选一个次梯度点作为返回值即可，因此实际函数实现是 **退化的**。

基类函数原型为：
```C++
Eigen::Vector3d support(const Eigen::Vector3d& direction, const Transform& transform);
```
其中 `transform` 是一个包含空间缩放、旋转、平移的变换结构体。在函数中，我们统一利用 `transform` 将给定的世界方向 `direction` 转换为局部方向 `localDir`，再将支撑函数所得到的局部支撑点通过 `transform` 变换到世界坐标系中。
### 长方盒体

给定标准长方体 $\mathcal{B} \subset \mathbb{R}^3$，边长分别为 $w, h, l \gt 0$，几何中心位于原点，长宽高分别与 $x, y, z$ 坐标轴平行：
$$
\mathcal{B} = [-\dfrac{w}{2}, \dfrac{w}{2}] \times [-\dfrac{h}{2}, \dfrac{h}{2}] \times [-\dfrac{l}{2}, \dfrac{l}{2}]
$$
给定任意的方向向量 $\mathbf{d} = \left( d_x, d_y, d_z \right) \in \mathbb{R}^3$，选定一个性质良好的唯一支撑点，即退化的支撑点函数（仅返回单点而非次微分集合）为：
$$
\mathbf{x}^*(\mathbf{d}) = 
\begin{pmatrix} 
\dfrac{w}{2} \cdot sgn{(d_x)} \\[8 px]
\dfrac{h}{2} \cdot sgn{(d_y)} \\[8 px]
\dfrac{l}{2} \cdot sgn{(d_z)} \\[8 px]
\end{pmatrix}
$$
其中符号函数 $sgn(t)$ 定义为：
$$
sgn(t) = 
\begin{cases} 
1,  t \gt 0 \\
-1, t \lt 0 \\
0, t =0
\end{cases}
$$
**C++实现：**
```C++
Eigen::Vector3d ShapeBox::support(const Eigen::Vector3d &direction, const Transform &transform) const  
{  
    // 将方向转换到局部坐标系  
    Eigen::Vector3d localDir = transform.getRotation().conjugate() * direction;  
  
    // 应用支撑点函数  
    const Eigen::Vector3d localSupport(  
        (localDir.x() > 0 ? 1 : -1) * (m_size.x() / 2.0),  
        (localDir.y() > 0 ? 1 : -1) * (m_size.y() / 2.0),  
        (localDir.z() > 0 ? 1 : -1) * (m_size.z() / 2.0));  
  
    // 变换回世界坐标系  
    return transform.localToWorld(localSupport);  
}
```

### 圆柱体

给定标准圆柱体 $\mathcal{C} \subset \mathbb{R}^3$，半径为 $r \gt 0$，母线长为 $h \gt 0$，几何中心位于原点，母线与 $z$ 轴平行：
$$
\mathcal{C} = \{ (x, y, z) | x^2 + y^2 \leq r^2, z \in [-\frac{h}{2}, \frac{h}{2}]\} 
$$
给定任意的方向向量 $\mathbf{d} = \left( d_x, d_y, d_z \right) \in \mathbb{R}^3$，选定一个性质良好的唯一支撑点，即退化的支撑点函数（仅返回单点而非次微分集合）为：
$$
\mathbf{x}^*(\mathbf{d}) =
\begin{cases}
\left(
r \dfrac{d_x}{\sqrt{d_x^2 + d_y^2}},\;
r \dfrac{d_y}{\sqrt{d_x^2 + d_y^2}},\; 
\dfrac{h}{2} \operatorname{sgn}(d_z) \right), 
& (d_x, d_y) \neq (0, 0), \\[10pt]
\left( 
0,\; 
0,\; 
\dfrac{h}{2} \operatorname{sgn}(d_z) \right), 
& (d_x, d_y) = (0, 0),
\end{cases}
$$

**C++ 实现**
```C++
Eigen::Vector3d ShapeCylinder::support(const Eigen::Vector3d& direction, const Transform& transform) const  
{  
    // 1. 将方向转换到局部坐标（仅旋转）  
    Eigen::Vector3d localDir = transform.getRotation().conjugate() * direction;  
  
    // 2. 归一化方向（避免数值问题）  
    const double len = localDir.norm();  
    if (len < 1e-12) {  
        // 零方向：返回任意支撑点（取上底面中心）  
        localDir = Eigen::Vector3d(0, 0, 1);  
    }  
    else {  
        localDir /= len;  
    }  
  
    // 3. 计算圆柱体支撑点（局部坐标）  
    const double halfH = m_height * 0.5;  
    constexpr double eps = 1e-12;  
  
    const double& dx = localDir.x();  
    const double& dy = localDir.y();  
    const double& dz = localDir.z();  
    const double xyNorm = sqrtSumOfSquares(dx, dy);  
  
    Eigen::Vector3d localSupport;  
  
    if (xyNorm < eps) {  
        // 方向沿 z 轴：支撑点在端面中心  
        localSupport = Eigen::Vector3d(0, 0, (dz >= 0 ? halfH : -halfH));  
    }  
    else {  
        // 水平方向单位向量  
        const double invXYNorm = 1.0 / xyNorm;  
        const double ux = dx * invXYNorm;  
        const double uy = dy * invXYNorm;  
  
        // 判断支撑点类型：比较 |dz| 与 xyNorm 的大小关系  
        // 注意：使用比较而非阈值，保证数学精确性  
        if (std::abs(dz) > xyNorm) {  
            // 方向更偏向端面：支撑点在顶/底边缘  
            const double zCoord = (dz >= 0 ? halfH : -halfH);  
            localSupport = Eigen::Vector3d(ux * m_radius, uy * m_radius, zCoord);  
        }  
        else {  
            // 方向更偏向侧面：支撑点在侧面（z 坐标由 dz/xyNorm 决定）  
            // 这里 dz/xyNorm ∈ [-1,1]   
            const double zCoord = (dz / xyNorm) * halfH; 
            localSupport = Eigen::Vector3d(ux * m_radius, uy * m_radius, zCoord);  
        }  
    }  
  
    // 4. 变换到世界坐标（仅旋转+平移，无缩放）  
    return transform.getRotation() * localSupport + transform.getTranslation();
}
```

### 圆盘

给定标准圆盘 $\mathcal{D} \subset \mathbb{R}^3$，半径为 $r \gt 0$，圆心位于原点，所在平面与 $Oxy$ 平面重合：
$$
\mathcal{D} = \{ (x, y, 0) | x^2 + y^2 \leq r^2\}
$$
给定任意的方向向量 $\mathbf{d} = \left( d_x, d_y, d_z \right) \in \mathbb{R}^3$，选定一个性质良好的唯一支撑点，即退化的支撑点函数（仅返回单点而非次微分集合）为：
$$
\mathbf{x}^*(\mathbf{d}) =
\begin{cases}
\left( 
r \dfrac{d_x}{\sqrt{d_x^2 + d_y^2}},\; 
r \dfrac{d_y}{\sqrt{d_x^2 + d_y^2}},\; 
0 \right), 
& (d_x, d_y) \neq (0, 0), \\[10pt]
(0,\; 0,\; 0), & (d_x, d_y) = (0, 0).
\end{cases}
$$
**C++ 实现**
```C++
Eigen::Vector3d ShapeDisk::support(const Eigen::Vector3d& direction, const Transform& transform) const  
{  
    // 将方向转换到局部坐标系（圆盘在局部坐标系中位于xy平面）  
    Eigen::Vector3d localDir = transform.getRotation().conjugate() * direction;  
  
    // 归一化方向  
    const double len = localDir.norm();  
    if (len < 1e-12) {  
        localDir = Eigen::Vector3d(1, 0, 0);  
    }  
    else {  
        localDir /= len;  
    }  
  
    // 圆盘的支撑点：在xy平面上沿投影方向取最大半径点  
    // 忽略z分量，因为圆盘是2D的（厚度为0）  
    Eigen::Vector2d xyDir(localDir.x(), localDir.y());  
    double xyLen = xyDir.norm();  
  
    Eigen::Vector3d localSupport;  
    if (xyLen > 1e-12) {  
        xyDir /= xyLen;  
        localSupport = Eigen::Vector3d(xyDir.x() * m_radius, xyDir.y() * m_radius, 0);  
    }  
    else {  
        // 方向垂直于圆盘平面，任意边缘点都可作为支撑点  
        localSupport = Eigen::Vector3d(m_radius, 0, 0);  
    }  
  
    // 变换回世界坐标系  
    return transform.localToWorld(localSupport);  
}

```

### 胶囊体

给定标准胶囊体 $\mathcal{Cs} \subset \mathbb{R}^3$，端半球半径为 $r \gt 0$，中央圆柱体的高为 $h \gt 0$，几何中心位于原点，母线与 $z$ 轴平行：
$$
\mathcal{Cs} = \{ (x, y, z) | \exists c \in [-\dfrac{h}{2}, \dfrac{h}{2}], [x^2 + y^2 + (z-c)^2 \leq r^2] \}
$$

设 $\mathbf{d} = (d_x, d_y, d_z) \in \mathbb{R}^3$ 为任意给定的方向向量，且满足归一化条件：
$$
\|\mathbf{d}\| = \sqrt{d_x^2 + d_y^2 + d_z^2} = 1.
$$
胶囊体不存在不可导的位置，给出支撑点函数为：
$$

\mathbf{x}^*(\mathbf{d}) =
\left(
r d_x,\;
r d_y,\;
r d_z + \frac{h}{2} \operatorname{sgn}(d_z)
\right)

$$
其中符号函数 $\operatorname{sgn}{\left(x\right)}$ 在前文已给出。 

利用胶囊体可以模拟一维线段在三维空间的 **近似支撑函数**。

**C++实现：**
```C++
Eigen::Vector3d ShapeCapsule::support(const Eigen::Vector3d &direction,  
                                      const Transform &transform) const  
{  
    // 1. 将方向从世界坐标转换到局部坐标（仅旋转，无需平移）  
    Eigen::Vector3d localDir = transform.getRotation().conjugate() * direction;  
  
    // 2. 归一化（避免数值问题）  
    const double len = localDir.norm();  
    if (len < 1e-12) {  
        localDir = Eigen::Vector3d(0.0, 0.0, 1.0);  
    } else {  
        localDir /= len;  
    }  
  
    // 3. 在局部坐标系中计算支撑点  
    //    x*(d) = (r·d_x, r·d_y, r·d_z + (h/2)·sgn(d_z))  
    const double dz = localDir.z();  
    const double sgn_dz = (dz > 0.0) ? 1.0 : ((dz < 0.0) ? -1.0 : 0.0);  
  
    Eigen::Vector3d localSupport;  
    localSupport.x() = m_radius * localDir.x();  
    localSupport.y() = m_radius * localDir.y();  
    localSupport.z() = m_radius * localDir.z() + m_height * 0.5 * sgn_dz;  
  
    // 4. 变换到世界坐标  
    return transform.getRotation() * localSupport + transform.getTranslation();  
}
```
### 线段（细杆）
给定标准线段 $\mathcal{L} \subset \mathbb{R}^3$，长度为 $l \gt 0$，中点位于原点，方向向量为 $(0, 0, 1)$。则在物理引擎的模拟下，假定其为一个端半球半径为 $\epsilon \gt 0$ 的胶囊体，且假定的半径 $\epsilon$ 满足下述条件：
$$
\left( \dfrac{\epsilon}{h} \lt 0.01 \space\right) \land  \left( \epsilon \leq 0.01\right)
$$
此时将线段转化为半径极小胶囊体，可利用胶囊体给出实现。

### 球体
给定标准球体 $\mathcal{S} \subset \mathbb{R}^3$，半径 $r \gt 0$，球心位于原点：
$$
\mathcal{S} = \{ \left( x, y, z\right) | \left( x^2 + y^2 + z^2\right) \leq r^2 \}
$$
设 $\mathbf{d} = (d_x, d_y, d_z) \in \mathbb{R}^3$ 为任意给定的方向向量，且满足归一化条件：
$$
\|\mathbf{d}\| = \sqrt{d_x^2 + d_y^2 + d_z^2} = 1.
$$
给出支撑点函数为：
$$
\mathbf{x}^*(\mathbf{d}) = r\cdot \mathbf{d}
$$
**C++实现：**
```C++
Eigen::Vector3d ShapeSphere::support(const Eigen::Vector3d& direction, const Transform& transform) const  
{  
    // 将方向转换到局部坐标系  
    Eigen::Vector3d localDir = transform.getRotation().conjugate() * direction;  
  
    // 归一化方向  
    const double len = localDir.norm();  
    if (len < 1e-12) {  
        localDir = Eigen::Vector3d(1, 0, 0);  
    }  
    else {  
        localDir /= len;  
    }  
  
    // 球体的支撑点：中心 + 方向 * 半径  
    Eigen::Vector3d localSupport = localDir * m_radius;  
  
    // 变换回世界坐标系  
    return transform.localToWorld(localSupport);  
}
```