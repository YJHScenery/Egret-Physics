# 基于 GJK 与 CCD 的凸体动态碰撞检测算法

## 1. 概述

在实时物理引擎中，碰撞检测是连接几何表示与物理响应的核心模块。传统的**离散碰撞检测**（Discrete Collision Detection）在固定时间步长 $\Delta t$ 内检查物体是否重叠，但当物体速度较快或几何形状较薄时，会产生**隧穿效应**（Tunneling Effect）：物体在 $t$ 时刻不重叠，在 $t + \Delta t$ 时刻已完全穿过彼此，导致碰撞完全漏检。

**连续碰撞检测**（Continuous Collision Detection, CCD）通过分析物体的运动轨迹，计算碰撞发生的精确时刻 $t_{toi}$（Time of Impact），从根本上避免穿透问题。

本文档介绍基于 **GJK 算法**（Gilbert-Johnson-Keerthi Distance Algorithm）实现 CCD 的完整技术方案，涵盖算法原理、实现细节及在时间步进积分中的应用。

---

## 2. GJK 算法与 Support 函数

### 2.1 Minkowski 和与分离超平面

GJK 算法的核心思想基于 **Minkowski 和**（或等价的 **Minkowski 差**）的几何性质。

给定两个凸体 $A$ 和 $B$，定义它们的 Minkowski 和为：
$$
A \oplus (-B) = \{a - b \mid a \in A, b \in B\}
$$

**关键定理**：两个凸体 $A$ 和 $B$ **不相交** 当且仅当原点 $\mathbf{0}$ 不在 Minkowski 和 $A \oplus (-B)$ 内；它们**相交** 当且仅当原点在该 Minkowski 和内部。

因此，碰撞检测问题被转化为：**判断原点是否在凸体 $A \oplus (-B)$ 内**。

这一转化的优势在于：如果存在一条从原点出发的不穿过 $A \oplus (-B)$ 的射线，则物体不相交，这条射线定义了**分离超平面**。

### 2.2 Support 函数

**Support 函数** $S_A(d)$ 是在给定方向 $\mathbf{d}$ 下，凸体 $A$ 上投影最远的点：
$$
S_A(\mathbf{d}) = \arg\max_{p \in A} \mathbf{d} \cdot p
$$

Support 函数具有以下关键性质：

1. **凸包保持**：对于任意方向 $\mathbf{d}$，$S_{A \oplus B}(\mathbf{d}) = S_A(\mathbf{d}) + S_B(\mathbf{d})$
2. **计算效率**：只需对具体形状实现 Support 函数，GJK 算法框架与形状无关
3. **对偶性**：$S_{-B}(\mathbf{d}) = -S_B(-\mathbf{d})$

对于 Minkowski 差 $A \oplus (-B)$，有：
$$
S_{A \oplus (-B)}(\mathbf{d}) = S_A(\mathbf{d}) - S_B(-\mathbf{d})
$$

### 2.3 GJK 算法流程

GJK 算法通过迭代构建一个不断逼近 $A \oplus (-B)$ 的单纯形（Simplex），判断原点是否在凸包内：

```
算法：GJK(A, B)
输入：凸体 A, B
输出：是否相交

1. 初始化：任选方向 d，构造包含 1 个顶点的初始单纯形
2. while (true) do
3.     在方向 d 下计算 A ⊕ (-B) 上的支持点 S(d)
4.     if S(d) · d ≤ 0 then
5.         return FALSE  // 原点在支持平面外，不相交
6.     end if
7.     将 S(d) 加入单纯形
8.     if containsOrigin(simplex) then
9.         return TRUE   // 原点在单纯形内，相交
10.    end if
11.    更新搜索方向 d 为从原点到单纯形最近点的方向
12. end while
```

### 2.4 单纯形包含原点检测

对于 2D 情况（三角形单纯形），检测原点是否在三角形内；对于 3D 情况（四面体单纯形），检测原点是否在四面体内。核心是计算原点到各边/面的距离，判断最近点。

### 2.5 EPA 算法：穿透深度与接触信息

当 GJK 判定物体相交后，需要计算**穿透深度**和**接触法线**。**EPA 算法**（Expanding Polytope Algorithm）在 GJK 的基础上，通过沿法线方向扩展单纯形来精确计算这些信息：

```
算法：EPA(A, B)
输入：GJK 找到的相交单纯形
输出：穿透深度 d, 接触法线 n

1. 从 GJK 的单纯形开始
2. while (true) do
3.     找到距离原点最近的三角形面 (Face)
4.     计算该面的 outward 法线 n
5.     在方向 n 下计算支持点 S(n)
6.     if |S(n) · n - d| < ε then
7.         return (d, n)  // 收敛
8.     end if
9.     将 S(n) 与所有可见边连接，重建单纯形
10. end while
```

---

## 3. 连续碰撞检测（CCD）原理

### 3.1 时间参数化的运动表示

在 CCD 中，每个物体的时间参数化运动定义为：
$$
\mathbf{p}_A(t) = \mathbf{p}_{A0} + \mathbf{v}_A t + \frac{1}{2}\mathbf{a}_A t^2 \quad \text{(线性运动)}
$$

$$
\mathbf{R}_A(t) = \mathbf{R}_{A0} \cdot \mathbf{q}(\bf{\omega}_A t) \quad \text{(旋转运动)}
$$

其中 $\mathbf{v}_A$ 是线性速度，$\bf{\omega}_A$ 是角速度。对于短时间步长，恒定速度近似是合理的。

### 3.2  swept 体积与 Time of Impact

两个运动物体 $A(t)$ 和 $B(t)$ 在时间区间 $[0, \Delta t]$ 内发生碰撞的条件是：存在 $t_{toi} \in [0, \Delta t]$ 使得 $A(t_{toi}) \cap B(t_{toi}) \neq \varnothing$。

这等价于判断原点是否在时变 Minkowski 和内：
$$
\bigcup_{t \in [0, \Delta t]} \left( A(t) \oplus (-B(t)) \right)
$$

### 3.3 保守扩展与二分搜索

直接计算时变 Minkowski 和是困难的。实践中采用**保守扩展**（Conservative Advancement）方法：

1. **初始检测**：使用初始位置 $A(0), B(0)$ 的 Minkowski 和 $A(0) \oplus (-B(0))$
2. **下界估计**：计算当前时刻两物体上沿相对速度方向的最近点对，其距离变化率给出 TOI 的下界
3. **二分搜索**：不断细化时间区间直到收敛到给定精度

本项目采用**二分搜索 + 离散碰撞检测**的混合方法：

```cpp
// 伪代码：连续碰撞检测
std::optional<double> continuousCollide(Shape& A, Shape& B, 
                                       Transform TA0, Transform TB0,
                                       Vector3 vA, Vector3 ωA,
                                       Vector3 vB, Vector3 ωB,
                                       double dt) {
    // t=0 时刻检测：已重叠则返回 0
    if (discreteCollide(A, TA0, B, TB0)) return 0.0;
    
    // t=dt 时刻检测：不相交则无碰撞
    Transform TAdt = integrate(TA0, vA, ωA, dt);
    Transform TBdt = integrate(TB0, vB, ωB, dt);
    if (!discreteCollide(A, TAdt, B, TBdt)) return std::nullopt;
    
    // 二分搜索 TOI
    double lo = 0.0, hi = dt;
    while (hi - lo > ε) {
        double mid = (lo + hi) / 2;
        Transform TAmid = integrate(TA0, vA, ωA, mid);
        Transform TBmid = integrate(TB0, vB, ωB, mid);
        if (discreteCollide(A, TAmid, B, TBmid)) {
            hi = mid;  // 碰撞在 [lo, mid] 内
        } else {
            lo = mid;  // 碰撞在 [mid, hi] 内
        }
    }
    return hi;
}
```

### 3.4 保守扩展的效率优化

保守扩展的关键是**紧的下界估计**。设相对速度为 $\mathbf{v}_{rel} = \mathbf{v}_A - \mathbf{v}_B$，两物体间的保守距离为 $d(t)$，则 TOI 满足：
$$
t_{toi} \approx \frac{d(0)}{|\mathbf{v}_{rel} \cdot \mathbf{n}|}
$$
其中 $\mathbf{n}$ 是当前分离方向的单位法线。

---

## 4. 接触流形（Contact Manifold）生成

### 4.1 接触特征分类

碰撞接触的几何特征分为三类：

| 接触类型 | 描述 | 接触点数量 |
|---------|------|-----------|
| **Face-Face** | 两个面的接触 | 多个点（典型 4 个） |
| **Edge-Face** | 棱与面的接触 | 2 个点 |
| **Vertex-Face** | 顶点与面的接触 | 1 个点 |
| **Edge-Edge** | 两棱的接触 | 1 个点（沿棱方向分布） |

### 4.2 接触点生成策略

对于凸多面体，接触点生成需要识别接触特征并计算精确的穿透深度。本项目采用以下策略：

1. **GJK 求交检测**：快速判断是否相交
2. **EPA 穿透求解**：获取分离超平面信息
3. **特征识别**：根据接触几何的局部拓扑分类接触类型
4. **点集采样**：对于面-面接触，沿重叠区域边界采样代表性接触点

### 4.3 接触数据定义

```cpp
struct ContactPoint {
    Vector3 position;     // 接触点世界坐标
    Vector3 normal;        // 接触法线（从 B 指向 A）
    double penetration;     // 穿透深度
    ContactFeature featureA; // A 上的接触特征类型
    ContactFeature featureB; // B 上的接触特征类型
};
```

**法线约定**：法线方向统一为从物体 B 指向物体 A，确保碰撞响应满足牛顿第三定律。

---

## 5. 基于 TOI 事件队列的时间步进积分

### 5.1 问题定义

在单个时间步长 $\Delta t$ 内，可能存在 $C_n^2$ 个潜在的碰撞对，每对返回一个 TOI。如何正确处理这些 TOI？

**朴素方法的缺陷**：
- 选择最小 TOI → 回溯 → 重新求解 → 重复
- 时间复杂度 $O(C_n^2 \cdot k)$，其中 $k$ 是迭代次数
- 当 $n$ 很大时，算力浪费严重

### 5.2 TOI 事件队列算法

核心思想：**收集所有 TOI → 按时间排序 → 依次处理**

```
算法：Time-Step with TOI Queue
输入：场景状态, 时间步长 dt
输出：更新后的场景状态

1. 收集阶段：
   - 对所有候选碰撞对执行 CCD
   - 收集所有 {toi, contact_manifold} 事件
   - 按 toi 升序排列事件队列

2. 时间步进循环：
   current_time = 0
   remaining_time = dt
   
   while (remaining_time > 0 AND !events.empty()) {
       event = events.front()
       toi = event.toi
       
       // 情况 1：最早碰撞在剩余时间之外
       if (toi > current_time + remaining_time) {
           integrate(remaining_time)  // 直接推进到帧末
           break
       }
       
       // 情况 2：碰撞发生在当前时刻
       time_to_collision = toi - current_time
       integrate(time_to_collision)  // 推进到碰撞时刻
       
       // 收集所有同一时刻的碰撞（批处理）
       batch = collectSimultaneousCollisions(events, toi, ε)
       
       // 批量求解接触约束
       resolveContacts(batch)
       
       current_time = toi
       remaining_time = dt - current_time
   }
   
   // 处理剩余时间
   if (remaining_time > 0) {
       integrate(remaining_time)
   }
```

### 5.3 批处理优化

同一时刻（或时间容差 $\varepsilon$ 内）的多个碰撞应该**一起求解**，因为：
1. 冲量会传播到其他物体，影响它们的相对速度
2. 分离求解会导致冲量累积顺序影响结果

```cpp
// 收集时间容差内的所有碰撞
std::vector<ContactConstraint> collectSimultaneousCollisions(
    std::vector<CcdCollisionEvent>& events,
    double target_toi,
    double epsilon = 1e-6) {
    
    std::vector<ContactConstraint> batch;
    while (!events.empty() && std::abs(events.front().toi - target_toi) < epsilon) {
        auto& event = events.front();
        for (const auto& contact : event.manifold) {
            batch.push_back({
                .bodyAIndex = event.bodyAIndex,
                .bodyBIndex = event.bodyBIndex,
                .contact = contact,
                .restitution = event.restitution,
                .toi = event.toi
            });
        }
        events.erase(events.begin());
    }
    return batch;
}
```

### 5.4 约束求解与冲量应用

对于每个接触约束，碰撞响应的法向冲量为：
$$
j_n = -(1 + e) \frac{\mathbf{v}_{rel} \cdot \mathbf{n}}{\frac{1}{m_A} + \frac{1}{m_B}}
$$

其中 $e$ 是恢复系数（restitution），$\mathbf{v}_{rel} = \mathbf{v}_B - \mathbf{v}_A$ 是相对速度。

**关键实现细节**：
1. 冲量更新后，必须重新计算速度导数 $\dot{\mathbf{S}}$，确保下一次 CCD 使用最新的速度
2. 位置修正（position correction）应在冲量求解后进行，避免穿透累积

---

## 6. 数值稳定性与工程实践

### 6.1 容差参数选择

| 参数 | 典型值 | 说明 |
|-----|-------|------|
| $\varepsilon_{time}$ | $10^{-10}$ | 时间精度容差 |
| $\varepsilon_{dist}$ | $10^{-8}$ | 距离精度容差 |
| $\varepsilon_{angle}$ | $10^{-8}$ | 角度精度容差 |
| $\varepsilon_{normal}$ | $10^{-12}$ | 法线归一化容差 |
| $k_{max\_iterations}$ | 80 | 二分搜索最大迭代 |

### 6.2 防止穿透的死循环

CCD 搜索可能因数值精度问题陷入死循环。通过以下策略确保终止：

1. **最大迭代限制**：防止极端情况下的无限循环
2. **自适应精度**：$\varepsilon = \max(10^{-10}, 10^{-6} \cdot dt)$
3. **退化检测**：当搜索区间过小时强制终止

### 6.3 高速物体的特殊处理

对于极端高速物体（$|\mathbf{v}| \cdot \Delta t > \text{几何尺寸}$），CCD 仍可能失效。解决方案：

1. **子步长细分**：当检测到高速时，自动将 $\Delta t$ 分为多个子步
2. **扫描球体**：使用 swept sphere 近似，牺牲一些精度换取稳定性
3. **最大速度限制**：在物理层限制物体最大速度

### 6.4 休眠机制

对于相对速度很小的物体对，可以将其标记为休眠，避免每帧重复求解：

```cpp
if (relativeVelocity.norm() < velocityThreshold && 
    penetrationDepth < penetrationThreshold) {
    markContactAsSleeping(contact);
}
```

---

## 7. 算法复杂度分析

### 7.1 GJK 算法复杂度

GJK 的收敛速度取决于初始方向选择和几何复杂度：
- **最好情况**：$O(1)$（形状已重叠）
- **平均情况**：$O(d)$，其中 $d$ 是单纯形维度（3D 下为 $O(1)$）
- **最坏情况**：$O(2^n)$，但实际中极少发生

### 7.2 CCD 总复杂度

对于 $n$ 个物体的场景：
- **广相位**：$O(n^2)$ 或使用空间哈希优化至 $O(n)$
- **CCD 检测**：$O(k_{pairs} \cdot k_{ccd})$，其中 $k_{ccd}$ 是二分搜索迭代次数（固定为 80）
- **约束求解**：$O(k_{contacts} \cdot k_{iterations})$

总复杂度在稀疏场景下接近 $O(n)$，在密集场景下为 $O(n^2)$。

---

## 8. 总结

基于 GJK 和 CCD 的碰撞检测算法通过以下核心思想实现可靠的实时物理模拟：

1. **GJK + EPA** 提供精确的相交检测和接触信息
2. **时间参数化搜索** 避免隧穿效应
3. **TOI 事件队列** 高效处理多碰撞场景
4. **批处理约束求解** 确保冲量一致性

该方案已在 Egret Physics 引擎中实现，支持 Box、Cylinder、Sphere、Disk、Rod 等凸几何体的 CCD 检测，并通过时间步进积分确保物理模拟的准确性。

---

## 参考文献

1. Gilbert, E. G., Johnson, D. W., & Keerthi, S. S. (1988). A fast procedure for computing the distance between complex objects in three-dimensional space.
2. Bergen, G. (2004). *Collision Detection in Interactive 3D Environments*.
3. Erin, C., et al. (2002). Adaptive Dynamic Walking for a Compass-like Biped Robot.
4. Redon, S., Kheddar, A., & Coiffet, P. (2002). Continuous Collision Detection for Adaptive Simulation of Articulated Bodies.
