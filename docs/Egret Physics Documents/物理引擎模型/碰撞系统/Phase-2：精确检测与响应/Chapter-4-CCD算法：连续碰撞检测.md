# CCD算法：连续碰撞检测

## 1. 问题陈述与算法目标

### 1.1 问题形式化

设 $A, B \subset \mathbb{R}^n$ 为两个非空紧致凸集，随时间 $t \in [0, T]$ 在 $n$ 维欧氏空间中运动。记 $A(t)$ 和 $B(t)$ 分别为 $A$ 和 $B$ 在时刻 $t$ 的空间占据区域。定义 **碰撞时间（Time of Impact，TOI）** 为：

$$
t^* = \inf \left\{ t \in [0, T] \;\middle|\; A(t) \cap B(t) \neq \varnothing \right\}
$$

若 $\{ \cdots \}$ 为空集，则定义 $t^* = +\infty$，表示在时间区间 $[0, T]$ 内两物体不发生碰撞。

**连续碰撞检测（Continuous Collision Detection，CCD）** 的目标是：给定 $A$、$B$ 在 $t = 0$ 时刻的完整运动状态以及 $[0, T]$ 内的运动规律，精确计算出 $t^*$ 以及 $t = t^*$ 时刻的碰撞信息（法线、穿透深度、接触点），或判定 $[0, T]$ 内无碰撞发生。

### 1.2 CCD 与静态检测的关系

静态 GJK+EPA 检测仅判断单一时刻的碰撞状态。当物体在 $[0, T]$ 内发生高速运动或快速旋转时，离散采样可能导致 **隧穿效应（tunneling）**——物体在相邻两帧之间完全穿透对方，静态检测无法捕获。

CCD 通过追踪物体在连续时间内的运动轨迹，保证不会遗漏任何碰撞事件。其与 GJK/EPA 的关系为：

- **GJK**：CCD 内层调用 GJK 以获取任意时刻 $t$ 的分离距离或碰撞判定。
- **EPA**：当 CCD 确定 $t^*$ 后，调用 EPA 提取该时刻的碰撞响应信息。

下图描绘 CCD 与静态 GJK/EPA 的调用关系：

```
                 ┌─────────────────────────────────────┐
                 │            CCD 主循环                │
                 │                                     │
                 │  ① 插值状态 A(t), B(t)              │
                 │         ↓                           │
                 │  ② 调用 GJK → 距离 d 或碰撞判定     │
                 │         ↓                           │
                 │  ③ 若碰撞 → 调用 EPA → 输出碰撞信息  │
                 │  ④ 若分离 → 计算保守步长 Δt         │
                 │         ↓                           │
                 │  ⑤ t += Δt，若 t < T 继续循环       │
                 └─────────────────────────────────────┘
```

### 1.3 算法设计目标

| 目标               | 说明                                                         |
| :----------------- | :----------------------------------------------------------- |
| **维度泛化**       | 算法适用 $\mathbb{R}^n$（$n \ge 2$），二维、三维为特例       |
| **运动模型可插拔** | 支持多种运动描述精度（从纯平移到含加速度的完整刚体运动）     |
| **无隧穿保证**     | 保守推进确保不会因离散采样而遗漏碰撞事件                     |
| **与 GJK/EPA 兼容** | 复用已有支撑函数体系和碰撞信息提取流程                       |
| **数值稳定**       | 对病态情形（擦边、初始穿透、极高角速度）提供明确的退化处理策略 |

---

## 2. 运动模型分级

在 $[0, T]$ 内，物体 $A$ 和 $B$ 的运动由其初始状态及运动参数完全确定。按描述精度从低到高分为三级模型。

### 2.1 共同前提

对于每个物体 $\mathcal{X} \in \{A, B\}$，其初始状态（$t=0$）为：

| 符号                                    | 含义           | 数学空间               |
| :------------------------------------ | :----------- | :----------------- |
| $\mathbf{x}_{\mathcal{X}}^0$          | 质心初始位置       | $\mathbb{R}^n$     |
| $R_{\mathcal{X}}^0$                   | 初始姿态矩阵       | $SO(n)$            |
| $\mathbf{v}_{\mathcal{X}}^0$          | 初始线速度        | $\mathbb{R}^n$     |
| $\boldsymbol{\Omega}_{\mathcal{X}}^0$ | 初始角速度（反对称矩阵） | $\mathfrak{so}(n)$ |

此外，每个物体具有正质量 $m_{\mathcal{X}} \in \mathbb{R}^+$ 和惯性张量 $\boldsymbol{\mathcal{I}}_{\mathcal{X}} : \mathfrak{so}(n) \to \mathfrak{so}(n)$。

### 2.2 模型 0：纯平移模型（LINEAR）

**假设**：在 $[0, T]$ 内，物体仅作匀速平移，姿态保持不变，角速度恒为零。

$$
\begin{aligned}
\mathbf{v}_{\mathcal{X}}(t) &= \mathbf{v}_{\mathcal{X}}^0 = \text{const} \\
\boldsymbol{\Omega}_{\mathcal{X}}(t) &= \mathbf{0} \\
R_{\mathcal{X}}(t) &= R_{\mathcal{X}}^0 \\
\mathbf{x}_{\mathcal{X}}(t) &= \mathbf{x}_{\mathcal{X}}^0 + \mathbf{v}_{\mathcal{X}}^0 \cdot t
\end{aligned}
$$

**适用场景**：低速平移、粒子系统、初筛阶段。速度上界仅需计算相对线速度。

### 2.3 模型 1：螺旋运动模型（SCREW）

**假设**：在 $[0, T]$ 内，线速度和角速度均为常数（零加速度），物体沿空间螺旋线运动。

$$
\begin{aligned}
\mathbf{v}_{\mathcal{X}}(t) &= \mathbf{v}_{\mathcal{X}}^0 = \text{const} \\
\boldsymbol{\Omega}_{\mathcal{X}}(t) &= \boldsymbol{\Omega}_{\mathcal{X}}^0 = \text{const} \\[6pt]
\mathbf{x}_{\mathcal{X}}(t) &= \mathbf{x}_{\mathcal{X}}^0 + \mathbf{v}_{\mathcal{X}}^0 \cdot t \\[6pt]
R_{\mathcal{X}}(t) &= R_{\mathcal{X}}^0 \cdot \exp\!\left( \boldsymbol{\Omega}_{\mathcal{X}}^0 \cdot t \right)
\end{aligned}
$$

**适用场景**：恒定角速度旋转（飞轮、陀螺）、大多数刚体模拟步。这是 CCD 的 **推荐默认模型**。

### 2.4 模型 2：加速运动模型（ACCELERATED）

**假设**：在 $[0, T]$ 内，线加速度和角加速度均为常数。线速度和角速度随时间线性变化。

$$
\begin{aligned}
\mathbf{v}_{\mathcal{X}}(t) &= \mathbf{v}_{\mathcal{X}}^0 + \mathbf{a}_{\mathcal{X}} \cdot t \\
\boldsymbol{\Omega}_{\mathcal{X}}(t) &= \boldsymbol{\Omega}_{\mathcal{X}}^0 + \boldsymbol{\alpha}_{\mathcal{X}} \cdot t \\[6pt]
\mathbf{x}_{\mathcal{X}}(t) &= \mathbf{x}_{\mathcal{X}}^0 + \mathbf{v}_{\mathcal{X}}^0 \cdot t + \frac{1}{2}\mathbf{a}_{\mathcal{X}} \cdot t^2 \\[6pt]
R_{\mathcal{X}}(t) &= R_{\mathcal{X}}^0 \cdot \exp\!\left( \boldsymbol{\Omega}_{\mathcal{X}}^0 \cdot t + \frac{1}{2}\boldsymbol{\alpha}_{\mathcal{X}} \cdot t^2 \right)
\end{aligned}
$$

其中 $\mathbf{a}_{\mathcal{X}} \in \mathbb{R}^n$ 为常线加速度，$\boldsymbol{\alpha}_{\mathcal{X}} \in \mathfrak{so}(n)$ 为常角加速度（反对称矩阵）。

**适用场景**：受力/力矩变化显著的场景、高精度物理模拟、TOI 精化阶段。

> **注意**：姿态更新式中指数映射的参数为反对称矩阵的积分 $\boldsymbol{\Theta}(t) = \boldsymbol{\Omega}^0 t + \tfrac{1}{2}\boldsymbol{\alpha} t^2$。该表达式忽略 $O(t^3)$ 的非对易项（$\boldsymbol{\Omega}^0$ 与 $\boldsymbol{\alpha}$ 一般不可交换），在 $t$ 为单帧时长（通常 $\ll 1$）时精度足够。严格形式需使用 Magnus 展开，参见 [[Chapter-3-牛顿定律推广：高维刚体运动模型#3.3 转动更新]]。

### 2.5 模型选择指南

| 模型       | 描述精度                     | 计算开销       | 典型用途                       |
| :--------- | :--------------------------- | :------------- | :----------------------------- |
| LINEAR     | 仅平移，忽略旋转             | 最低           | 初筛、粒子、低速刚体           |
| SCREW      | 含恒速旋转，忽略加速度       | 中等（推荐）   | 通用刚体 CCD、单帧内检测       |
| ACCELERATED | 含加速度，二阶精度           | 最高           | 高精度 TOI、受力突变场景        |

---

## 3. 核心原理：保守推进

### 3.1 闵可夫斯基差的时间演化

定义时刻 $t$ 的闵可夫斯基差为：

$$
C(t) = A(t) \ominus B(t) = \left\{ \mathbf{a} - \mathbf{b} \;\middle|\; \mathbf{a} \in A(t),\; \mathbf{b} \in B(t) \right\}
$$

$C(t)$ 是 $\mathbb{R}^n$ 中的紧致凸集。两物体在时刻 $t$ 碰撞 $\iff$ 原点 $\mathbf{0} \in C(t)$。

定义 **分离距离函数**：

$$
d(t) = \operatorname{dist}\!\big(\mathbf{0},\; C(t)\big) = \min_{\mathbf{p} \in C(t)} \|\mathbf{p}\|
$$

当 $d(t) = 0$ 时物体恰好接触；$d(t) > 0$ 时分离；严格 $\mathbf{0} \in \operatorname{int}(C(t))$ 时穿透。

### 3.2 利普希茨连续性与保守推进定理

**引理 1（距离函数的利普希茨连续性）** 设 $C(t)$ 中的每个点以不超过 $L$ 的速率运动，则 $d(t)$ 是 $L$-利普希茨连续的：

$$
|d(t_2) - d(t_1)| \le L \cdot |t_2 - t_1|, \quad \forall t_1, t_2 \in [0, T]
$$

**证明概要**：设 $t_1$ 时刻最近点为 $\mathbf{p}_1^* \in C(t_1)$，$\|\mathbf{p}_1^*\| = d(t_1)$。在 $t_2$ 时刻，$\mathbf{p}_1^*$ 对应的点以速度不超过 $L$ 运动到 $\mathbf{p}_1' \in C(t_2)$，满足 $\|\mathbf{p}_1' - \mathbf{p}_1^*\| \le L|t_2 - t_1|$。由三角不等式：

$$
d(t_2) \le \|\mathbf{p}_1'\| \le \|\mathbf{p}_1^*\| + L|t_2 - t_1| = d(t_1) + L|t_2 - t_1|
$$

对称地交换 $t_1 \leftrightarrow t_2$ 即得 $|d(t_2) - d(t_1)| \le L|t_2 - t_1|$。$\square$

**定理 1（保守推进定理）** 设 $d(0) > 0$，且 $C(t)$ 上任意点的运动速率不超过常数 $L > 0$。若选取步长：

$$
\Delta t \le \frac{d(t)}{L}
$$

则 $d(t + \Delta t) \ge 0$，即在该步内不会越过原点（不会遗漏碰撞）。

**证明**：由引理 1，$d(t + \Delta t) \ge d(t) - L \cdot \Delta t \ge d(t) - L \cdot \frac{d(t)}{L} = 0$。$\square$

### 3.3 速度上界 $L$ 的一般形式

$C(t)$ 中任意点 $\mathbf{p} = \mathbf{a} - \mathbf{b}$ 的速度为：

$$
\dot{\mathbf{p}} = \dot{\mathbf{a}} - \dot{\mathbf{b}}
$$

对物体 $\mathcal{X} \in \{A, B\}$，其表面上任意点 $\mathbf{p}_{\mathcal{X}}$ 的速度可分解为质心线速度与绕质心旋转的线速度之和：

$$
\dot{\mathbf{p}}_{\mathcal{X}} = \mathbf{v}_{\mathcal{X}} + \boldsymbol{\Omega}_{\mathcal{X}} \cdot (\mathbf{p}_{\mathcal{X}} - \mathbf{c}_{\mathcal{X}})
$$

其中 $\mathbf{c}_{\mathcal{X}}$ 为 $\mathcal{X}$ 的质心（或几何中心）。因此对任意 $\mathbf{a} \in A, \mathbf{b} \in B$：

$$
\begin{aligned}
\|\dot{\mathbf{a}} - \dot{\mathbf{b}}\|
&\le \|\mathbf{v}_A - \mathbf{v}_B\| + \|\boldsymbol{\Omega}_A \cdot (\mathbf{a} - \mathbf{c}_A)\| + \|\boldsymbol{\Omega}_B \cdot (\mathbf{b} - \mathbf{c}_B)\| \\[6pt]
&\le \|\mathbf{v}_A - \mathbf{v}_B\| + \|\boldsymbol{\Omega}_A\| \cdot r_A + \|\boldsymbol{\Omega}_B\| \cdot r_B
\end{aligned}
$$

其中：

- $r_{\mathcal{X}} = \max_{\mathbf{p} \in \mathcal{X}} \|\mathbf{p} - \mathbf{c}_{\mathcal{X}}\|$ 为物体 $\mathcal{X}$ 的 **包围半径**（质心到最远点的距离）。
- $\|\boldsymbol{\Omega}_{\mathcal{X}}\|$ 为反对称矩阵 $\boldsymbol{\Omega}_{\mathcal{X}}$ 的 **谱范数**（最大奇异值），即该物体在任意二维旋转平面上的最大角速度。

由此可得普适的速度上界：

$$
\boxed{L = \|\mathbf{v}_A - \mathbf{v}_B\| + \|\boldsymbol{\Omega}_A\| \cdot r_A + \|\boldsymbol{\Omega}_B\| \cdot r_B}
$$

---

## 4. 速度上界计算

### 4.1 包围半径

对于常见凸体基元，包围半径 $r = \max_{\mathbf{p} \in \mathcal{X}} \|\mathbf{p} - \mathbf{c}\|$ （$\mathbf{c}$ 为几何中心）的计算如下：

| 形状类型   | 包围半径 $r$                                      |
| :--------- | :------------------------------------------------ |
| 球体       | 球半径 $R$                                        |
| 长方盒体   | $\frac{1}{2}\sqrt{w^2 + h^2 + l^2}$              |
| 圆柱体     | $\sqrt{r_{\text{底}}^2 + (h/2)^2}$               |
| 圆盘       | 圆盘半径 $R$                                      |
| 胶囊体     | $R + h/2$                                         |
| 任意凸体   | $\max_{\mathbf{v} \in \text{Vertices}} \|\mathbf{v} - \mathbf{c}\|$ |

对于由支撑函数描述的任意凸体，包围半径可通过在所有坐标轴方向（正负各一）采样支撑点并取最大距离来逼近。具体地：

$$
r \approx \max_{\mathbf{d} \in \mathcal{D}} \|\mathbf{x}^*(\mathcal{X}, \mathbf{d}) - \mathbf{c}\|
$$

其中 $\mathcal{D}$ 为一组采样方向（例如 $n$ 维标准基的正负方向，共 $2n$ 个）。

### 4.2 反对称矩阵的谱范数

对 $\boldsymbol{\Omega} \in \mathfrak{so}(n)$，其谱范数 $\|\boldsymbol{\Omega}\|_2$ 表示该旋转在任意二维平面上产生的最大角速度。

**谱分解形式**：任意实反对称矩阵可正交对角块化为：

$$
Q^T \boldsymbol{\Omega} Q = \operatorname{blockdiag}\!\left(
\begin{bmatrix} 0 & -\theta_1 \\ \theta_1 & 0 \end{bmatrix},\;
\begin{bmatrix} 0 & -\theta_2 \\ \theta_2 & 0 \end{bmatrix},\;
\dots,\;
\begin{bmatrix} 0 & -\theta_k \\ \theta_k & 0 \end{bmatrix},\;
0,\dots,0
\right)
$$

其中 $k = \lfloor n/2 \rfloor$，$\theta_i \ge 0$ 为各旋转平面上的角速度大小。则：

$$
\|\boldsymbol{\Omega}\|_2 = \max_{1 \le i \le k} \theta_i
$$

**三维退化**：当 $n = 3$ 时，$\boldsymbol{\Omega}$ 通过霍奇对偶映射为轴矢量 $\boldsymbol{\omega} \in \mathbb{R}^3$，此时 $\|\boldsymbol{\Omega}\|_2 = \|\boldsymbol{\omega}\|$。

**实用上界——Frobenius 范数**：若谱范数不便直接计算，可使用 Frobenius 范数作为上界：

$$
\|\boldsymbol{\Omega}\|_2 \le \|\boldsymbol{\Omega}\|_F = \sqrt{\sum_{i=1}^{n}\sum_{j=1}^{n} \Omega_{ij}^2} = \sqrt{2\sum_{i<j} \Omega_{ij}^2}
$$

Frobenius 范数最多高估 $\sqrt{\lfloor n/2 \rfloor}$ 倍（所有旋转平面角速度相等时），使 CCD 步长相应缩短，**保证安全性但可能增加迭代次数**。对于 $n=3$，高估因子为 $\sqrt{2} \approx 1.414$；对于 $n=2$，高估因子为 $1$（Frobenius 范数等于谱范数）。

### 4.3 LINEAR 模型速度上界

LINEAR 模型下 $\boldsymbol{\Omega}_A = \boldsymbol{\Omega}_B = \mathbf{0}$，上式退化为：

$$
\boxed{L_{\text{LINEAR}} = \|\mathbf{v}_A - \mathbf{v}_B\|}
$$

$L_{\text{LINEAR}}$ 在 $[0, T]$ 内为常数，无需逐次重算。

### 4.4 SCREW 模型速度上界

SCREW 模型下 $\mathbf{v}_{\mathcal{X}}$ 和 $\boldsymbol{\Omega}_{\mathcal{X}}$ 均为常数：

$$
\boxed{L_{\text{SCREW}} = \|\mathbf{v}_A - \mathbf{v}_B\| + \|\boldsymbol{\Omega}_A\|_2 \cdot r_A + \|\boldsymbol{\Omega}_B\|_2 \cdot r_B}
$$

$L_{\text{SCREW}}$ 在 $[0, T]$ 内亦为常数，**可在循环外预计算一次**。

### 4.5 ACCELERATED 模型速度上界

ACCELERATED 模型下速度随时间变化：

$$
\begin{aligned}
\mathbf{v}_{\mathcal{X}}(t) &= \mathbf{v}_{\mathcal{X}}^0 + \mathbf{a}_{\mathcal{X}} \cdot t \\
\boldsymbol{\Omega}_{\mathcal{X}}(t) &= \boldsymbol{\Omega}_{\mathcal{X}}^0 + \boldsymbol{\alpha}_{\mathcal{X}} \cdot t
\end{aligned}
$$

因此在时刻 $t$ 的瞬时速度上界为：

$$
L(t) = \|\mathbf{v}_A(t) - \mathbf{v}_B(t)\| + \|\boldsymbol{\Omega}_A(t)\|_2 \cdot r_A + \|\boldsymbol{\Omega}_B(t)\|_2 \cdot r_B
$$

对于从 $t$ 到 $t + \Delta t$ 的推进步，需使用 **该区间上的最大速率**：

$$
L_{[t,\, t+\Delta t]} = \max_{\tau \in [t,\, t+\Delta t]} L(\tau)
$$

**极值定位**：因 $\mathbf{v}_{\mathcal{X}}(\tau)$ 和 $\boldsymbol{\Omega}_{\mathcal{X}}(\tau)$ 均为 $\tau$ 的仿射函数，且范数是凸函数，故 $\|\mathbf{v}_A(\tau)-\mathbf{v}_B(\tau)\|$ 和 $\|\boldsymbol{\Omega}_{\mathcal{X}}(\tau)\|_2$ 均在区间端点处取得最大值。因此：

$$
\boxed{L_{[t,\, t+\Delta t]} = \max\!\big(L(t),\; L(t + \Delta t)\big)}
$$

**步长求解**：由于 $L_{[t,\,t+\Delta t]}$ 依赖于未知的 $\Delta t$，采用预估-校正策略：

1. **预估步**：用当前速率计算试探步长 $\displaystyle \Delta t^{(0)} = \frac{d(t)}{L(t)}$
2. **校正步**：计算终点速率 $L(t + \Delta t^{(0)})$，取区间最大速率：
   $$
   L_{\max} = \max\!\big(L(t),\; L(t + \Delta t^{(0)})\big)
   $$
3. **重算步长**：$\displaystyle \Delta t^{(1)} = \frac{d(t)}{L_{\max}}$
4. **收敛检查**：若 $|\Delta t^{(1)} - \Delta t^{(0)}| / \Delta t^{(0)} < 0.1$，接受 $\Delta t^{(1)}$；否则令 $\Delta t^{(0)} \leftarrow \Delta t^{(1)}$，返回步骤 2（通常一次迭代即收敛）。

> **工程简化**：当 $T$ 很小时（单物理帧），加速度引起的速度变化可忽略，直接用 $L(t)$ 近似 $L_{[t,\,t+\Delta t]}$ 也可接受，代价是失去"无隧穿"的严格保证。

---

## 5. 运动状态插值

CCD 主循环中需要频繁获取任意时刻 $t \in [0, T]$ 的物体状态。统一的状态插值函数如下：

### 5.1 平动插值

对模型 LINEAR 和 SCREW（匀速）：

$$
\mathbf{x}_{\mathcal{X}}(t) = \mathbf{x}_{\mathcal{X}}^0 + \mathbf{v}_{\mathcal{X}}^0 \cdot t
$$

对模型 ACCELERATED（匀加速）：

$$
\mathbf{x}_{\mathcal{X}}(t) = \mathbf{x}_{\mathcal{X}}^0 + \mathbf{v}_{\mathcal{X}}^0 \cdot t + \frac{1}{2}\mathbf{a}_{\mathcal{X}} \cdot t^2
$$

### 5.2 转动插值

对模型 LINEAR：

$$
R_{\mathcal{X}}(t) = R_{\mathcal{X}}^0 \quad (\text{恒定})
$$

对模型 SCREW（恒角速度）：

$$
R_{\mathcal{X}}(t) = R_{\mathcal{X}}^0 \cdot \exp\!\left( \boldsymbol{\Omega}_{\mathcal{X}}^0 \cdot t \right)
$$

对模型 ACCELERATED（匀角加速度）：

$$
R_{\mathcal{X}}(t) = R_{\mathcal{X}}^0 \cdot \exp\!\left( \boldsymbol{\Omega}_{\mathcal{X}}^0 \cdot t + \frac{1}{2}\boldsymbol{\alpha}_{\mathcal{X}} \cdot t^2 \right)
$$

### 5.3 组合状态插值函数

```text
函数 interpolateState(state_0, t, model):
    // state_0 = { x0, R0, v0, Ω0, a, α }
    
    若 model == LINEAR:
        x_t = state_0.x0 + state_0.v0 · t
        R_t = state_0.R0
    
    若 model == SCREW:
        x_t = state_0.x0 + state_0.v0 · t
        Θ   = state_0.Ω0 · t
        R_t = state_0.R0 · exp(Θ)
    
    若 model == ACCELERATED:
        x_t = state_0.x0 + state_0.v0 · t + 0.5 · state_0.a · t²
        Θ   = state_0.Ω0 · t + 0.5 · state_0.α · t²
        R_t = state_0.R0 · exp(Θ)
    
    返回 (x_t, R_t)
```

> **指数映射实现**：$\exp(\Theta)$ 对反对称矩阵 $\Theta$ 的计算在 $n=2$ 和 $n=3$ 时有闭式解（Rodrigues 公式）。对于更高维度，使用矩阵指数的截断级数展开或 Pade 近似。当 $\|\Theta\| \ll 1$ 时，$\exp(\Theta) \approx I + \Theta + \frac{1}{2}\Theta^2$ 已足够精确。

---

## 6. 算法完整流程

### 6.1 符号汇总

| 符号           | 含义                                                |
| :------------- | :-------------------------------------------------- |
| $A, B$         | 两个凸体（通过支撑函数 $\mathbf{x}^*$ 描述）        |
| $\text{st}_A, \text{st}_B$ | 初始运动状态结构体                                  |
| $\text{model}$ | 运动模型：LINEAR / SCREW / ACCELERATED              |
| $T$            | 检测时间区间长度（通常为一物理帧长 $\Delta t_{\text{physics}}$） |
| $\varepsilon_d$ | 分离距离容差（低于此值视为接触）                     |
| $\varepsilon_v$ | 速度上界容差（低于此值视为无接近趋势）               |
| $\delta_{\min}$ | 最小时间步长（低于此值终止，防止无限循环）           |
| $t$            | 当前累积时间                                        |
| $d$            | 当前时刻 GJK 返回的分离距离                          |
| $L$            | 当前有效的速度上界                                  |

### 6.2 主算法

```text
函数 CCD(A, B, st_A, st_B, model, T) → CCDResult:

    // ── 预处理 ──
    r_A = computeBoundingRadius(A)       // 参见 §4.1
    r_B = computeBoundingRadius(B)

    // 预计算 SCREW 速度上界（LINEAR / SCREW 模型全程不变）
    若 model == LINEAR 或 model == SCREW:
        L_const = computeVelocityBound(st_A, st_B, 0, model, r_A, r_B)
        若 L_const < ε_v: 返回 NoCollision

    // ── 主循环 ──
    t = 0

    当 t < T 时循环:

        // ① 插值当前时刻的状态
        (x_A, R_A) = interpolateState(st_A, t, model)
        (x_B, R_B) = interpolateState(st_B, t, model)

        // ② 调用 GJK 计算分离距离或碰撞判定
        gjkResult = GJK(A @ (x_A, R_A), B @ (x_B, R_B))

        若 gjkResult.collision == true:
            // 已穿透 → 调用 EPA 提取碰撞信息
            epaResult = EPA(gjkResult.σ, gjkResult.S_A, gjkResult.S_B)
            返回 CCDResult(collision=true, TOI=t, contact=epaResult)

        // ③ 提取分离距离
        d = gjkResult.d₀

        若 d < ε_d:
            // 距离极小 → 视为接触
            contact = extractContactFromGJKClosestPoints(gjkResult)
            返回 CCDResult(collision=true, TOI=t, contact=contact)

        // ④ 确定速度上界
        若 model == LINEAR 或 model == SCREW:
            L = L_const
        否则若 model == ACCELERATED:
            L = computeVelocityBound(st_A, st_B, t, model, r_A, r_B)
            若 L < ε_v: 返回 NoCollision

            // 预估-校正（参见 §4.5）
            Δt_candidate = d / L
            Δt_candidate = min(Δt_candidate, T - t)
            L_end  = computeVelocityBound(st_A, st_B, t + Δt_candidate, model, r_A, r_B)
            L      = max(L, L_end)

        若 L < ε_v:
            返回 NoCollision    // 无接近趋势

        // ⑤ 保守时间步长
        Δt = d / L
        Δt = min(Δt, T - t)    // 不超过剩余时间

        若 Δt < δ_min:
            返回 NoCollision    // 步长过小，判定无碰撞

        // ⑥ 推进
        t = t + Δt

    // ── 循环结束仍未碰撞 ──
    返回 NoCollision
```

### 6.3 子函数：速度上界计算

```text
函数 computeVelocityBound(st_A, st_B, t, model, r_A, r_B) → L:

    // 获取 t 时刻的瞬时速度
    (v_A, Ω_A) = getVelocity(st_A, t, model)
    (v_B, Ω_B) = getVelocity(st_B, t, model)

    v_rel_norm = ||v_A - v_B||
    Ω_A_norm   = spectralNorm(Ω_A)    // 或用 frobeniusNorm(Ω_A)
    Ω_B_norm   = spectralNorm(Ω_B)

    返回 v_rel_norm + Ω_A_norm · r_A + Ω_B_norm · r_B


函数 getVelocity(st, t, model) → (v, Ω):

    若 model == LINEAR:
        返回 (st.v0, 零矩阵)

    若 model == SCREW:
        返回 (st.v0, st.Ω0)

    若 model == ACCELERATED:
        v = st.v0 + st.a · t
        Ω = st.Ω0 + st.α · t
        返回 (v, Ω)
```

### 6.4 接触信息提取（分离距离极小但未穿透情形）

当 $d < \varepsilon_d$ 而 GJK 报告未碰撞时，物体间存在极小的正分离距离。此时原点不在 $C(t)$ 内部，无法直接调用 EPA（EPA 要求原点在多面体内部）。改为从 GJK 的最近点信息直接构造接触数据：

由 GJK 的最终单纯形及其子集 $S_{\sigma}^*$（参见 [[Chapter-1-GJK算法：静态碰撞检测#4. 单纯形剔除]]），可获得：

- $\mathbf{p}^*$：$C(t)$ 上距原点最近的点（$\|\mathbf{p}^*\| = d$）
- $\mathbf{a}_0$、$\mathbf{b}_0$：$A$、$B$ 上的一对最近点

则接触信息构造为：

$$
\begin{aligned}
\mathbf{n} &= \frac{\mathbf{p}^*}{\|\mathbf{p}^*\|} \quad \text{（分离法线，方向为 B 指向 A）} \\[6pt]
\delta &= 0 \quad \text{（穿透深度为零，或设为 } -d \text{ 表示分离）} \\[6pt]
\mathbf{c} &= \frac{\mathbf{a}_0 + \mathbf{b}_0}{2} \\[6pt]
\mathbf{a}^* &= \mathbf{a}_0,\quad \mathbf{b}^* = \mathbf{b}_0
\end{aligned}
$$

### 6.5 TOI 精化（可选后处理）

主循环返回的 TOI 可能存在 $\Delta t$ 量级的误差（保守推进可能在接触发生前就因 $d < \varepsilon_d$ 触发终止）。如需更高精度的 TOI，可在主循环检测到碰撞后执行 **二分搜索** 或 **割线法** 精化：

```text
函数 refineTOI(A, B, st_A, st_B, model, t_lo, t_hi, max_iter):

    // t_lo: 已知分离的时刻（d > ε_d）
    // t_hi: 已知碰撞的时刻（collision == true 或 d < ε_d）
    // 二分搜索至 |t_hi - t_lo| < δ_tol 或 max_iter 耗尽

    对 iter = 1 到 max_iter:
        t_mid = (t_lo + t_hi) / 2
        (x_A, R_A) = interpolateState(st_A, t_mid, model)
        (x_B, R_B) = interpolateState(st_B, t_mid, model)
        gjkResult = GJK(A @ (x_A, R_A), B @ (x_B, R_B))

        若 gjkResult.collision 或 gjkResult.d₀ < ε_d:
            t_hi = t_mid    // 碰撞侧
        否则:
            t_lo = t_mid    // 分离侧

    返回 t_hi
```

> **调用时机**：主循环返回 `collision=true` 且 `TOI=t` 后，以 `[t - Δt_last, t]` 为初始区间调用 `refineTOI`，其中 `Δt_last` 为最后一次推进的步长。精化完成后在精确 TOI 处重新调用 EPA 获取碰撞信息。

---

## 7. 数据处理与接口

### 7.1 输入数据结构

```text
结构体 MotionState:
    x0 :  ℝⁿ          // 初始位置
    R0 :  SO(n)       // 初始姿态（n×n 正交矩阵，det=+1）
    v0 :  ℝⁿ          // 初始线速度
    Ω0 :  𝔰𝔬(n)       // 初始角速度（n×n 反对称矩阵）
    a  :  ℝⁿ          // 常线加速度（ACCELERATED 模型需提供，否则为零）
    α  :  𝔰𝔬(n)       // 常角加速度（ACCELERATED 模型需提供，否则为零）

结构体 Shape:
    support(direction, transform) → ℝⁿ    // 支撑点函数
    boundingRadius → ℝ                     // 包围半径（可预计算）
```

### 7.2 输出数据结构

```text
结构体 ContactInfo:
    n      : ℝⁿ     // 碰撞法线（单位向量，从 B 指向 A）
    δ      : ℝ      // 穿透深度（≥ 0 表示穿透，= 0 表示恰好接触）
    c      : ℝⁿ     // 接触点世界坐标（取 A、B 接触点中点）
    a_star : ℝⁿ     // 物体 A 上的接触点世界坐标
    b_star : ℝⁿ     // 物体 B 上的接触点世界坐标

结构体 CCDResult:
    collision : bool          // 是否发生碰撞
    TOI       : ℝ             // 碰撞时间（0 ≤ TOI ≤ T），未碰撞时为 +∞
    contact   : ContactInfo   // 碰撞信息（仅在 collision=true 时有效）
```

### 7.3 与 GJK/EPA 的调用协议

CCD 在以下情形调用 GJK 和 EPA：

| 调用点               | 调用目的                                   | 输入                                   | 输出用途                                   |
| :------------------- | :----------------------------------------- | :------------------------------------- | :----------------------------------------- |
| 主循环每步           | `GJK(A(t), B(t))` 获取分离距离或碰撞判定    | 经插值的物体状态                         | 判定接触、获取 $d$ 用于计算步长            |
| 检测到碰撞时         | `EPA(σ, S_A, S_B)` 提取穿透信息             | GJK 返回的单纯形与支撑点集              | 生成 `ContactInfo`                        |
| $d < \varepsilon_d$ 时 | 直接使用 GJK 最近点信息（不调用 EPA）      | GJK 返回的 $\mathbf{a}_0, \mathbf{b}_0$ | 构造分离法线和近接触点                     |
| TOI 精化（可选）     | 反复调用 `GJK(A(t_mid), B(t_mid))` 二分搜索 | 中间时刻的插值状态                       | 定位精确 TOI                               |

---

## 8. 边界情形与退化处理

### 8.1 初始已穿透

若 $t = 0$ 时刻两物体已处于穿透状态（GJK 立即返回碰撞），CCD 无法通过保守推进回溯。处理策略：

1. **直接委托 EPA**：在 $t = 0$ 处运行 EPA，获取当前穿透信息返回。
2. **回溯试探**（可选）：以极小的负时间步 $-\delta t$ 回退至分离状态，再正向 CCD 定位 TOI。此策略仅在需要精确 TOI 时使用，通常运动学穿透由位置校正层修复。

```text
若 t == 0 且 gjkResult.collision:
    epaResult = EPA(gjkResult.σ, gjkResult.S_A, gjkResult.S_B)
    返回 CCDResult(collision=true, TOI=0, contact=epaResult)
```

### 8.2 平行擦过

当两物体以几乎平行的轨迹擦肩而过时，距离函数 $d(t)$ 的极小值非常接近零但不为零。保守推进在此情形下会反复以极小步长接近该极小值，可能导致迭代次数膨胀。

**检测条件**：连续 $N_{\text{stall}}$ 次迭代满足 $\Delta t < \delta_{\min} \cdot K$（$K$ 为一个小倍数，如 $K = 10$）但 $d > \varepsilon_d$。

**处理策略**：

1. 记录最近几次迭代的最小距离 $d_{\min}^{\text{history}}$。
2. 若 $d$ 在多次迭代中反复增大（$\Delta d / \Delta t > 0$），说明已越过最接近点。
3. 判定为无碰撞，返回 `NoCollision`。

```text
// 平行擦过检测（嵌入主循环中）
若 iter_count > N_stall:
    若 d > d_min_history 且 d 连续增大:
        返回 NoCollision    // 已越过最接近点，确认无碰撞
d_min_history = min(d_min_history, d)
```

### 8.3 数值容差策略

CCD 涉及多个容差参数，需根据空间尺度合理设定：

| 参数             | 推荐值                    | 含义                             |
| :--------------- | :------------------------ | :------------------------------- |
| $\varepsilon_d$ | $10^{-6} \cdot L_{\text{scene}}$ | 分离距离容差（场景特征长度的 $10^{-6}$） |
| $\varepsilon_v$ | $10^{-10}$                | 速度零阈值（机器精度量级）       |
| $\delta_{\min}$  | $10^{-8} \cdot T$         | 最小时间步长（总时长的 $10^{-8}$） |
| $\delta_{\text{tol}}$ | $10^{-6} \cdot T$     | TOI 精化二分容差                 |
| $N_{\text{stall}}$ | $20$                    | 平行擦过检测的停滞阈值           |

**维度缩放**：上述推荐值适用于 $n = 3$ 且场景尺度 $\sim 1$ 的情形。对于 $n$ 维空间，需注意距离和速度的量纲保持一致。包围半径 $r_{\mathcal{X}}$ 在 $n$ 维下通常随 $\sqrt{n}$ 缩放（例如 $n$ 维单位超立方体的包围半径为 $\sqrt{n}/2$），但速度上界公式中的量纲关系不变。

### 8.4 极高角速度

当 $\|\boldsymbol{\Omega}\| \cdot r \gg \|\mathbf{v}\|$ 时，旋转主导速度上界，保守步长可能极小，导致迭代次数大幅增加。缓解策略：

- **多分辨率包围半径**：对非球形物体，可计算"到质心不同距离"层次上的包围半径，在旋转不敏感的迭代阶段使用外层包围半径。
- **降级模型**：若 ACCELERATED 模型下角加速度导致步长过小，可临时退回 SCREW 模型（忽略角加速度对速度上界的贡献），仅在实际检测到接触时精化。

---

## 9. 附录：三维退化验证

本节将 $n$ 维公式退化至 $n = 3$，验证与经典三维 CCD 文献的一致性。

### 9.1 速度上界退化

$n=3$ 时，$\boldsymbol{\Omega} \in \mathfrak{so}(3)$，通过霍奇对偶 $\boldsymbol{\Omega} \mapsto \boldsymbol{\omega} \in \mathbb{R}^3$（$\Omega_{ij} = -\varepsilon_{ijk}\,\omega_k$），谱范数退化为：

$$
\|\boldsymbol{\Omega}\|_2 = \|\boldsymbol{\omega}\|
$$

SCREW 速度上界退化为：

$$
L = \|\mathbf{v}_A - \mathbf{v}_B\| + \|\boldsymbol{\omega}_A\| \cdot r_A + \|\boldsymbol{\omega}_B\| \cdot r_B
$$

这正是经典三维保守推进 CCD 中使用的速度上界形式。

### 9.2 姿态插值退化

指数映射 $\exp(\boldsymbol{\Omega} t)$ 在 $n=3$ 时等价于 Rodrigues 旋转公式。令 $\boldsymbol{\omega} = \theta \cdot \hat{\mathbf{u}}$（$\theta$ 为转角，$\hat{\mathbf{u}}$ 为单位转轴）：

$$
\exp(\boldsymbol{\Omega} t) = I + \sin(\theta t) \cdot \mathbf{U} + (1 - \cos(\theta t)) \cdot \mathbf{U}^2
$$

其中 $\mathbf{U}$ 为 $\hat{\mathbf{u}}$ 对应的反对称矩阵。这与标准三维旋转矩阵构造完全一致。

### 9.3 GJK/EPA 退化

GJK 的单纯形在 $n=3$ 时为四面体（4 点）、三角形（3 点）、线段（2 点）或单点（1 点），与经典三维 GJK 一致。EPA 的面退化为三角形，面列表退化为三角剖分，与经典三维 EPA 一致。

### 9.4 二维退化

当 $n=2$ 时，$SO(2)$ 为单参数群，$\mathfrak{so}(2)$ 由一个标量角速度描述：

$$
\boldsymbol{\Omega} = \begin{bmatrix} 0 & -\omega \\ \omega & 0 \end{bmatrix}, \quad \|\boldsymbol{\Omega}\|_2 = |\omega|
$$

GJK 在 $\mathbb{R}^2$ 上的单纯形最多包含 3 个顶点（三角形），EPA 的面为一维边。此时速度上界退化为：

$$
L = \|\mathbf{v}_A - \mathbf{v}_B\| + |\omega_A| \cdot r_A + |\omega_B| \cdot r_B
$$

与二维 CCD 文献完全一致。

---

## 10. 总结：CCD 执行流程总图

```
 输入: A, B (凸体), st_A, st_B (运动状态), model, T
  │
  ├─ 预处理: 计算 r_A, r_B; 预计算 L_const (LINEAR/SCREW)
  │
  ▼
  t = 0
  │
  ▼
  ┌─────────────────────────────────────────┐
  │            主循环: 当 t < T              │
  │                                         │
  │  ① interpolateState(t) → A(t), B(t)    │
  │  ② GJK(A(t), B(t)) → 碰撞?/距离 d      │
  │                                         │
  │  ├─ 碰撞 → EPA → 返回碰撞信息           │
  │  ├─ d < ε_d → GJK最近点 → 返回接触信息  │
  │  │                                      │
  │  └─ 分离:                               │
  │      ③ computeVelocityBound(t) → L      │
  │      ④ Δt = d / L (ACCELERATED: 预估-校正)│
  │      ⑤ Δt = min(Δt, T - t)              │
  │      ⑥ 若 Δt < δ_min → 返回 NoCollision │
  │      ⑦ t += Δt → 继续循环              │
  └─────────────────────────────────────────┘
  │
  ▼
 返回 NoCollision
  │
  ▼
 (可选) TOI精化: 二分搜索 [t-Δt, t] → 精确 TOI + EPA
```

本算法通过保守推进框架统一了 $n$ 维空间的连续碰撞检测，三级运动模型覆盖了从低精度快速筛选到高精度 TOI 计算的完整需求谱系。所有组件——支撑函数、GJK、EPA、运动学插值——均以维度泛化形式定义，确保算法在任意有限维欧氏空间中保持一致的逻辑结构和数学正确性。
