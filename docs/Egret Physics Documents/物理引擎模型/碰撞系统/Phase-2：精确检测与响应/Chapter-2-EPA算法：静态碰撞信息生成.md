# EPA算法：静态碰撞信息生成

## 概念与符号约定

从 GJK 阶段，我们获得了下述的GJK数据信息：
$$
\operatorname{GJK} = \set{\text{collision}, \mathbf{V}_{\sigma}, \set{S_A, S_B}, \set{d_0, \mathbf{a}_0, \mathbf{b}_0}}
$$
符号定义详见 [[Chapter-1-GJK算法：静态碰撞检测#数据处理]]

**EPA 算法（Expanding Polytope Algorithm）** 以 GJK 的最终单纯形矩阵 $\mathbf{V}_{\sigma}$ 为初始多面体，通过迭代扩展，逼近闵可夫斯基差 $A \ominus B$ 的边界，从而提取碰撞响应所需要的完整信息。

**生成物理量**

| 物理量  | 符号           | 数学定义                                                                                      | 用途          |
| ---- | ------------ | ----------------------------------------------------------------------------------------- | ----------- |
| 碰撞法线 | $\mathbf{n}$ | $\mathbf{n} = \dfrac{\mathbf{p}}{\mathbf{\|p\|}}, \mathbf{p} \in \partial{(A \ominus B)}$ | 冲量方向确定      |
| 穿透深度 | $\delta$     | $\delta = \| \mathbf{p}\|$                                                                | 位置修正与细分时间确认 |
| 接触点  | $\mathbf{c}$ | $\mathbf{c} = \dfrac{\mathbf{a}^* + \mathbf{b}^*}{2}$                                     | 作用位置        |
其中，$\mathbf{a}^* \in A, \mathbf{b}^* \in B$，满足 $\mathbf{p} = \mathbf{a}^* - \mathbf{b}^*$。

**表述约定** 下文中所提到的 **多面体** 泛指任意大于等于2的整数 $m$ 维空间中由 $m-1$ 维超平面所构成的封闭凸集。
## 原理

### 问题转化

EPA 的核心目标是在闵可夫斯基差 $A \ominus B$ 的边界 $\partial{C}$ 上找到距离远点最近的边界点 $\mathbf{p}^*$：
$$
\mathbf{p}^* = \arg{\min_{\mathbf{p} \in \partial{C}}{\|\mathbf{p}\|}}
$$
由于 $C$ 是凸集，上述问题等价于在凸多面体表面求解一个带约束的优化问题——在欧氏距离意义下，找到原点在凸体边界上的最近投影点。

**几何直观：** 若原点在凸体内部，原点最接近的边界点一定位于某个面上，且该面法向量方向与从原点到该面垂线的方向一致。

---
### KKT 条件与迭代规则

**KKT条件（Karush-Kuhn-Tucker条件）** 是优化理论中的一组必要条件，适用于求解带有等式和不等式约束的非线性规划问题。当目标函数和约束条件是凸的时，KKT条件也是找到最优解的充分条件。它是对经典的拉格朗日乘子法的扩展，用于处理带有不等式约束的优化问题

设当前多面体的一个候选面 $\Gamma$ 具有法向量 $\mathbf{n}_{\Gamma}$ 和到原点的距离纯量 $d_{\Gamma} \gt 0$。面 $\Gamma$ 上距离原点最近的点 $\mathbf{p}_{\Gamma}$ 满足：
- $\mathbf{p}_{\Gamma}$ 在面 $\Gamma$ 所在的平面上：$\mathbf{n}_{\Gamma} \cdot \mathbf{p}_{\Gamma} = d_{\Gamma}$
- $\mathbf{p}_{\Gamma}$ 与法向量 $\mathbf{n}_{\Gamma}$ 同向共线：$\mathbf{p}_{\Gamma} = d_{\Gamma} \cdot \mathbf{n}_{\Gamma}$

EPA通过在所有候选面中选择**距离原点最近的面**（即 $\min{d_{\Gamma}}$​），在该面的法向量方向上获取新的支撑点来扩展多面体，逐步逼近真实最近边界。

---
## 流程

### 符号约定

- $C = A \ominus B$：闵可夫斯基差
- $\mathcal{P}$ 当前多面体（初始为 GJK 的最终单纯形 $\sigma$，可用矩阵$\mathbf{V}$ 表述）
- $\mathcal{F}$ 多面体 $\mathcal{P}$ 的面列表，每个面存储为顶点的索引列表。在 $n$ 维空间中，每 $n$ 个顶点可以确定一个超平面。
- $\mathbf{n}_{\Gamma}$ 面 $\Gamma$ 的单位法向量（指向多面体的外部）
- $d_{\Gamma}$ 面 $\Gamma$ 到原点的距离：$d_f = \mathbf{n}_{\Gamma} \cdot \mathbf{v}$，其中 $\mathbf{v}$ 为面上的任意顶点
- $\mathbf{x}_m^*(A, B, \mathbf{d})$ 闵可夫斯基差在方向 $\mathbf{d}$ 上的支撑点

---
### 1. 初始化

从 GJK 阶段获取的最终单纯形为 $\sigma = \set{\mathbf{v}_0, \mathbf{v}_1, \cdots, \mathbf{v}_k}$，其中 $k \leq n$。等价的矩阵表述为 $\mathbf{V}_{n, k+1}$
1. 令 $\mathcal{P} = \sigma$
2. 计算 $\mathcal{P}$ 的所有面 $\mathcal{F}$
3. 对每个面 $\Gamma \in \mathcal{F}$ ，计算法向量 $\mathbf{n}_{\Gamma}$ 和距离 $d_{\Gamma}$

**注意** 所有面的法向量需统一指向**原点所在的一侧**（即多面体外部方向），确保 $d_{\Gamma} \geq 0$ 始终成立。 

---
### 2. 主迭代循环

重复下述步骤，直到满足终止条件：

---
#### 步骤1. 寻找最近平面

在当前多面体 $\mathcal{P}$ 的所有面中，选择到原点距离最小的面：
$$
\Gamma^* = \arg{\min_{\Gamma \in \mathcal{F}}}
$$
令 $d_{min} = d_{\Gamma^*}$，$\mathbf{n} = \mathbf{n}_{\Gamma}$。

---
#### 步骤2. 获取新的支撑点

沿法向量方向获取闵可夫斯基差的新支撑点为：
$$
\mathbf{p} = \mathbf{x}_m^*(A, B, \mathbf{n}) = \mathbf{x}^*(A, \mathbf{n}) - \mathbf{x}^*(B, \mathbf{-n})
$$
记录对应的原始支撑点 $\mathbf{a} = \mathbf{x}^*(A, \mathbf{d})$ 和 $\mathbf{b} = \mathbf{x}^*(B, \mathbf{-d})$ 

---
#### 步骤3. 终止条件检查

如果新的支撑点 $\mathbf{p}$ 在面 $\Gamma^*$ 的外侧没有越过该面，即满足：
$$
\mathbf{p} \cdot \mathbf{n} - d_{min} \leq 0
$$
（注：工程上通常使用容差 $\epsilon$ 代替不等式右侧的 $0$ ）

则说明 $\mathbf{p}$ 没有使多面体向原点方向继续扩展，当前多面体已充分逼近闵可夫斯基差的边界。

此时终止迭代。令 $\mathbf{p}^* = d_{min} \cdot \mathbf{n}$。

若条件未满足，则进入下述步骤。

---
#### 步骤4. 多面体拓展

将新的支撑点 $\mathbf{p}$ 加入多面体：
$$
\mathcal{P} = \mathcal{P} \cup \set{\mathbf{p}}
$$
更新多面体的三角剖分，生成新的面列表 $\mathcal{F}$，剔除被 $\mathbf{p}$ 遮挡的面，构建新的凸包。

**具体操作**：
- 识别从 $\mathbf{p}$ 可见的面（法向量方向与 $\mathbf{p}$ 背离原点的面）
- 移除这些面
- 用 $\mathbf{p}$ 与可见面的边界边构建新的三角形/面

---
#### 步骤5. 更新记录

存储新的支撑点 $\mathbf{p}$ 对应的原始支撑点对 $(\mathbf{a}, \mathbf{b})$ 以备用。

返回步骤1，继续迭代。

### 3. 碰撞信息提取

迭代终止后，得到边界上的最近点：
$$
\mathbf{p}^* = d_{min} \cdot \mathbf{n}
$$
其中 $d_{min}$ 是最近面的距离，$\mathbf{n}$ 是此最近面的法向量。
**1. 碰撞法线**

$$
\mathbf{n}_c = \mathbf{n}
$$
**2. 穿透深度**
$$
\delta = d_{min}
$$
**3. 接触点**
利用最终最近面 $\Gamma^*$  的重心坐标向量和 GJK 保留的原始支撑点信息 $S_A, S_B$：

设最近面 $\Gamma^*$ 的顶点为 $\sigma^* = \set{\mathbf{v}_{i_0}, \mathbf{v}_{i_1}, \cdots , \mathbf{v}_{i_m}}$，其对应的原始支撑点对为 $\set{(\mathbf{a}_{i_j}, \mathbf{b}_{i_j})}$ 。

求解最近点 $\mathbf{p}^*$ 在该面上的重心坐标 $\mathbf{\Lambda}$：
$$
\mathbf{p}^* = \sum_{j = 0}^{m}{\lambda_j\mathbf{v}_{i_j}}, \quad\sum_{j = 0}^{m}{\lambda_j} = 1, \quad \forall j: \lambda_j \geq 0

$$
则原始物体上的接触点为：
$$
\mathbf{a}^* = \sum_{j = 0}^{m}{\lambda_j\mathbf{a}_{i_j}} \quad \mathbf{b}^* = \sum_{j = 0}^{m}{\lambda_j\mathbf{b}_{i_j}}
$$
最终接触点取中点：
$$
\mathbf{c} = \dfrac{\mathbf{a}^* + \mathbf{b}^*}{2}
$$


## 数据处理

EPA 阶段将会返回下面的数据：
$$
\text{EPA} = \set{\mathbf{n}, \delta, \mathbf{c}, \set{\mathbf{a}^* , \mathbf{b}^*}}
$$
在低速运动且数值不敏感的领域，GJK+EPA 的碰撞检测算法已然能够满足绝大部分的需求。但是为了 **1. 避免高速运动情形下的穿透；2. 实现数值的精确可控**，通过这些数据，我们进入连续碰撞检测 CCD 算法阶段：[[Chapter-4-CCD算法：连续碰撞检测]]

# 附录1：单纯形计算

定义单纯形 $\sigma \to \mathbf{V}_{n, k+1}$ 定义了一个 $n$ 维 $k$ 阶单纯形凸包，其中 $\sigma$ 为单纯形的所有顶点构成的集合，$\mathbf{V}_{n, k+1}$ 为 $\sigma$ 中每一个点作为 **列向量**，按照一定顺序排列形成的矩阵。下面给出 $\sigma$ 的所有面的生成、法向量、原点距离的计算公式

**关键约束** GJK 算法保证了此单纯形包含原点。

## 面生成

单纯形 $\sigma$ 的所有 $k-1$ 维超平面构成集合：
$$
\mathcal{F} = \set{\Gamma_{i}: i = 0, 1, \cdots, k}
$$
其中：
$$
\Gamma_i = \operatorname{conv}\{ \mathbf{v}_j: j \neq i, j = 0, 1, \cdots, k\}
$$
**注** 符号 $\text{conv}$ 是 $\text{span}$ 满足一定约束条件的特殊情形，具体如下
$$
\operatorname{conv}(S) = \left\{ \sum_{i=0}^{k} \lambda_i \mathbf{v}_i \;\middle|\; \lambda_i \geq 0,\; \sum_{i=0}^{k} \lambda_i = 1 \right\}
$$
## 法向量计算

令 $\mathbf{V}_i$ 为 $\mathbf{V}$ 删去第 $i$ 列之后得到的矩阵：
$$
\mathbf{V}_i = 
\begin{bmatrix} 
\mathbf{v}_0 & \mathbf{v}_1 & \cdots & \mathbf{v}_{i - 1}  & \mathbf{v}_{i + 1} & \cdots & \mathbf{v}_k
\end{bmatrix}
$$
记 $\mathbf{1}_m$ =  $\begin{pmatrix} 1 & 1 & \cdots & 1 \end{pmatrix}^T \in \mathbb{R}^m$。

### 满维情形

若 $k = n$，则对每一个 $i = 0, \cdots, k$，解线性方程组：
$$
\mathbf{V}_i^T \mathbf{n}_i = \mathbf{1}_n
$$
即：
$$
\mathbf{n}_i = (\mathbf{V_i^T})^{-1}\mathbf{1}_n
$$
**（满维且包含原点的单纯形所生成的 $\mathbf{V}_i$ 一定可逆）**

则 $\Gamma_i$ 所在的超平面为 
$$
\mathbf{n}_i^T\mathbf{x} = 1
$$
单位外法向量：
$$
\mathbf{u}_i= \dfrac{\mathbf{n}_i}{\| \mathbf{n}_i\|}
$$
原点到该超平面的距离为：
$$
d_i = \dfrac{1}{\|\mathbf{n}_i\|}
$$
因此超平面也可以写为：
$$
\mathbf{u}_i^T \mathbf{x} = d_i
$$
### 非满维情形

此时，包含 $\Gamma_i$ 的超平面不唯一，若取“最小范数”意义下的法向量，则可令：
$$
\mathbf{n}_i = \mathbf{V}_i \left( \mathbf{V}_i^T \mathbf{V}_i \right)^+\mathbf{1}_k
$$
其中，$\left( \mathbf{V}_i^T \mathbf{V}_i \right)^+$ 是矩阵 $\mathbf{V}_i^T \mathbf{V}_i$ 的 **Moore-Penrose 广义逆（伪逆）**（参见：[[#附录2：Moore-Penrose 广义逆]]）。于是：
$$
\Gamma_i \subset \set{\mathbf{x}: \mathbf{n}_i^T\mathbf{x} = 1}
$$
且原点到 $\Gamma_i$ 的距离为：
$$
d_i = \dfrac{1}{\|\mathbf{n}_i\|} = \dfrac{1}{\sqrt{ \mathbf{1}_k^T\left( \mathbf{V}_i^T \mathbf{V}_i \right)^+ \mathbf{1}_k} }
$$
单位外法向量为：
$$
\mathbf{u}_i= \dfrac{\mathbf{n}_i}{\| \mathbf{n}_i\|}
$$
# 附录2：Moore-Penrose 广义逆

## 定义

对于任意矩阵 $A \in \mathbb{C}^{m \times n}$，其 Moore-Penrose 广义逆 $A^+ \in \mathbb{C}^{n \times m}$ 是满足以下四个 **Penrose 条件**的唯一矩阵：

1. $A A^+ A = A$
2. $A^+ A A^+ = A^+$
3. $(A A^+)^* = A A^+ \quad (\text{对称/共轭对称})$
4. $(A^+ A)^* = A^+ A \quad (\text{对称/共轭对称})$

其中 $(\cdot)^*$ 表示共轭转置，即：$A^* = (\overline{A})^T$，$\overline{A}$  表示将矩阵 $A$ 中的元素 $a_{i, j} \in \mathbb{C}$ 改为其共轭复数 $\overline{a_{i,j}} \in \mathbb{C}$。

---
## 基于 SVD 分解的计算公式

若矩阵 $A$ 的奇异值分解（SVD）为：

$$
A = U \Sigma V^*
$$

其中：
- $U \in \mathbb{C}^{m \times m}$，$V \in \mathbb{C}^{n \times n}$ 是酉矩阵
- $\Sigma \in \mathbb{R}^{m \times n}$ 是对角矩阵，对角元为奇异值 $\sigma_1 \ge \sigma_2 \ge \cdots \ge \sigma_r > 0$，$r = \text{rank}(A)$

则广义逆为：

$$
A^+ = V \Sigma^+ U^*
$$

其中 $\Sigma^+$ 是 $\Sigma$ 的广义逆：将非零奇异值取倒数，零奇异值保持不变（为零）：

$$
\Sigma^+ = 
\begin{bmatrix}
\sigma_1^{-1} & & & & \\
& \ddots & & & \\
& & \sigma_r^{-1} & & \\
& & & 0 & \\
& & & & \ddots
\end{bmatrix}_{n \times m}
$$

---

## 特殊情况

| 条件 | 公式 |
| :--- | :--- |
| $A$ 列满秩（$m \ge n$，$\text{rank}(A) = n$） | $A^+ = (A^* A)^{-1} A^*$ |
| $A$ 行满秩（$n \ge m$，$\text{rank}(A) = m$） | $A^+ = A^* (A A^*)^{-1}$ |
| $A$ 是可逆方阵 | $A^+ = A^{-1}$ |
| $A = 0$（零矩阵） | $A^+ = 0$（零矩阵，尺寸为 $n \times m$） |

---

## 核心性质

### 极限形式（Tikhonov 正则化）

$$
A^+ = \lim_{\alpha \downarrow 0} (A^* A + \alpha I)^{-1} A^* = \lim_{\alpha \downarrow 0} A^* (A A^* + \alpha I)^{-1}
$$

### 其他重要性质

$$
\begin{aligned}
&(A^+)^+ = A \\
&(A^*)^+ = (A^+)^* \\
&(A^+ A)^2 = A^+ A, \quad (A A^+)^2 = A A^+ \quad (\text{幂等性}) \\
&\text{rank}(A^+) = \text{rank}(A)
\end{aligned}
$$