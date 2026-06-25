# 牛顿定律推广：高维刚体运动模型

设定背景：考虑一个 n 维欧氏空间 $\mathbb{R}^n$，刚体的旋转不再绕"轴"发生，而是发生在由任意两个坐标轴张成的二维平面内。旋转自由度总数为 $N = \dfrac{n(n-1)}{2}$。

---

## 1. 物理量与数学结构

### 1.1 位置与姿态

| 物理量    | 数学空间                   | 自由度                 | 几何性质           |
| :----- | :--------------------- | :------------------ | :------------- |
| **位置** | $\mathbb{R}^n$（n 维向量空间） | $n$                 | 平坦、线性、加法群      |
| **姿态** | $SO(n)$（特殊正交群）         | $\dfrac{n(n-1)}{2}$ | 弯曲、非线性、乘法群（李群） |

**位置** $\mathbf{x} \in \mathbb{R}^n$，是一个 n 维列向量。**质量** $m \in \mathbb{R}^+$，是一个大于零的纯量。

**姿态**由**旋转矩阵** $R$ 描述：

$$ R \in SO(n) \quad \Longleftrightarrow \quad R^T R = I_n,\quad \det(R) = +1 $$

其中 $I_n$ 是 $n \times n$ 单位矩阵。$R$ 的每一列代表刚体固连坐标系各基矢在当前空间坐标系中的投影。

> **关键区别**：位置 $\mathbf{x}$ 是"加法量"（位移直接相加），而姿态 $R$ 是"乘法量"（两次旋转 $R_1$ 和 $R_2$ 复合为 $R_1 R_2$）。

### 1.2 速度与加速度

- **速度** $\mathbf{v} = \dfrac{d\mathbf{x}}{dt} \in \mathbb{R}^n$，是位置对时间的一阶导数。
- **加速度** $\mathbf{a} = \dfrac{d^{2}\mathbf{x}}{dt^{2}} \in \mathbb{R}^n$，是位置对时间的二阶导数。

### 1.3 角速度与角加速度

在高维空间中，旋转发生在二维平面上，因此角速度不再是一个矢量，而是一个 **二阶反对称张量**（即李代数 $\mathfrak{so}(n)$ 的元素）。

- **角速度** $\boldsymbol{\Omega} \in \mathfrak{so}(n)$，即一个 $n \times n$ 的实反对称矩阵。分量满足 $\Omega_{ij} = -\Omega_{ji}$（$i, j = 1, 2, \dots, n$）。$\Omega_{ij}$ 表示在 $ij$ 平面上的旋转角速度大小。
- **角加速度** $\boldsymbol{\alpha} = \dfrac{d\boldsymbol{\Omega}}{dt} \in \mathfrak{so}(n)$，同样是 $n \times n$ 的实反对称矩阵。分量满足 $\alpha_{ij} = -\alpha_{ji}$，表示各转动平面上角速度的变化率。

### 1.4 力与力矩

- **力** $\mathbf{F} \in \mathbb{R}^n$，是一个 n 维向量，表示作用在刚体上的合外力。
- **力矩**：三维空间中力矩是位置矢量与力的叉乘（$\boldsymbol{\tau} = \mathbf{r} \times \mathbf{F}$）。在高维空间中，叉积被 **外积（楔积）** 的反对称部分所取代。
  $\boldsymbol{\tau} \in \mathfrak{so}(n)$ 是一个 $n \times n$ 的反对称矩阵。设力的作用点相对于参考点（通常为质心）的位置矢量为 $\mathbf{r} \in \mathbb{R}^n$，力为 $\mathbf{F} \in \mathbb{R}^n$，则：
	$$ \boldsymbol{\tau} = \mathbf{r} \wedge \mathbf{F} = \mathbf{r} \mathbf{F}^T - \mathbf{F} \mathbf{r}^T $$

  分量形式：$\tau_{ij} = r_i F_j - r_j F_i$，正好是三维叉乘对应反对称矩阵的推广。

### 1.5 惯性张量

高维空间中，惯性张量是将"角速度"映射为"角动量"的线性算子。由于角速度是 $\mathfrak{so}(n)$ 中的反对称矩阵，惯性张量是一个 **四阶张量**，或者等价地看作 $\mathfrak{so}(n)$ 空间上的一个线性变换。

**四阶张量分量形式**：

$$ I_{ijkl} = \frac{1}{2} \int_V \rho(\mathbf{r}) \left( r_i r_l \delta_{jk} - r_j r_l \delta_{ik} - r_i r_k \delta_{jl} + r_j r_k \delta_{il} \right) d^n r $$

其中 $\rho(\mathbf{r})$ 是密度，$\delta$ 是克罗内克符号。

**对称性**：
- $I_{ijkl} = -I_{jikl}$（对前两指标反对称）
- $I_{ijkl} = -I_{ijlk}$（对后两指标反对称）
- $I_{ijkl} = I_{klij}$（前后指标对调对称）

**映射关系**：角动量 $\mathbf{L} \in \mathfrak{so}(n)$ 与角速度 $\boldsymbol{\Omega}$ 的关系为：

$$ L_{ij} = \sum_{k=1}^n \sum_{l=1}^n I_{ijkl} \, \Omega_{kl} $$

**低维特例（$n=3$）**：当 $n=3$ 时，可利用霍奇对偶将反对称矩阵映射为三维矢量，四阶张量退化为 $3 \times 3$ 的对称矩阵（即常见的惯性矩阵）。

### 1.6 物理量汇总

| 序号 | 物理量  | 符号                         | 数学结构                              | 维度/空间                                   |
| :--: | :--- | :------------------------- | :-------------------------------- | :-------------------------------------- |
|  0   | 位置   | $\mathbf{x}$               | 向量                                | $\mathbb{R}^n$                          |
|  0   | 质量   | $m$                        | 正标量                               | $\mathbb{R}^+$                          |
|  1   | 速度   | $\mathbf{v}$               | 向量                                | $\mathbb{R}^n$                          |
|  2   | 加速度  | $\mathbf{a}$               | 向量                                | $\mathbb{R}^n$                          |
|  3   | 角速度  | $\boldsymbol{\Omega}$      | 二阶反对称张量（矩阵）                       | $\mathfrak{so}(n)$                      |
|  4   | 角加速度 | $\boldsymbol{\alpha}$      | 二阶反对称张量（矩阵）                       | $\mathfrak{so}(n)$                      |
|  5   | 力    | $\mathbf{F}$               | 向量                                | $\mathbb{R}^n$                          |
|  6   | 力矩   | $\boldsymbol{\tau}$        | 二阶反对称张量（矩阵）                       | $\mathfrak{so}(n)$                      |
|  7   | 惯性张量 | $\boldsymbol{\mathcal{I}}$ | 四阶张量（或 $\mathfrak{so}(n)$ 上的线性算子） | $\mathfrak{so}(n) \to \mathfrak{so}(n)$ |

---

## 2. 运动学与动力学

在此模型下，平动与转动完全解耦：
- **力 $\mathbf{F}$ 仅决定质心的平动加速度。**
- **力矩 $\boldsymbol{\tau}$ 仅决定绕质心的角加速度。**
### 2.1 平动（牛顿第二定律）

$$ \mathbf{F} = m \, \mathbf{a} $$

即 $\dfrac{d\mathbf{p}}{dt} = \mathbf{F}$，其中 $\mathbf{p} = m\mathbf{v}$ 为线动量。

### 2.2 转动运动学（姿态的微分方程）

类比速度是位置的导数（$\mathbf{v} = \dfrac{d\mathbf{x}}{dt}$），姿态的"速度"是旋转矩阵对时间的导数 $\dfrac{dR}{dt}$。

但 $\dfrac{dR}{dt}$ 本身并非独立的物理量（它有 $n^2$ 个分量却只有 $\dfrac{n(n-1)}{2}$ 个自由度）。因此，引入**角速度** $\boldsymbol{\Omega}$ 来唯一描述姿态的变化率：

$$ \dfrac{dR}{dt} = R(t) \cdot \boldsymbol{\Omega}(t) $$

其中 $\boldsymbol{\Omega}(t) \in \mathfrak{so}(n)$ 是 $n \times n$ 的实反对称矩阵。$\Omega_{ij}$ 表示刚体在当前瞬时、在 $ij$ 平面上的旋转角速度。

这等价于将体坐标系中的瞬时旋转速度（$\boldsymbol{\Omega}$）映射回空间坐标系，得到空间中的变化率。若采用空间坐标系下的角速度 $\boldsymbol{\Omega}_{\text{space}}$，则有 $\dfrac{dR}{dt} = \boldsymbol{\Omega}_{\text{space}} \cdot R$。通常选用体坐标系形式（$\dfrac{dR}{dt} = R \boldsymbol{\Omega}$），因为惯性张量在体坐标系中为常数。

### 2.3 转动动力学（高维欧拉方程）

姿态的动力学由 **角动量守恒定律** 推广而来，其核心骨架与牛顿定律完全对偶：

| 平动（位置）                                           | 转动（姿态）                                                                                |
| :----------------------------------------------- | :------------------------------------------------------------------------------------ |
| 线动量：$\mathbf{p} = m \mathbf{v} \in \mathbb{R}^n$ | 角动量：$\mathbf{L} = \boldsymbol{\mathcal{I}}[\boldsymbol{\Omega}] \in \mathfrak{so}(n)$ |
| 牛顿定律：$\dfrac{d\mathbf{p}}{dt} = \mathbf{F}$             | 转动定律：$\dfrac{d\mathbf{L}}{dt} = \boldsymbol{\tau}$                                           |

将角动量的定义代入转动定律，即得 **n 维欧拉方程**：

$$ \boldsymbol{\mathcal{I}}\left[\dfrac{d\boldsymbol{\Omega}}{dt}\right] + \left[ \boldsymbol{\Omega}, \ \boldsymbol{\mathcal{I}}[\boldsymbol{\Omega}] \right] = \boldsymbol{\tau} $$

其中：
- $\boldsymbol{\mathcal{I}}\left[\dfrac{d\boldsymbol{\Omega}}{dt}\right]$ 是惯性张量对角加速度的线性映射，类比于 $m\mathbf{a}$（惯性项）。
- $\left[ \boldsymbol{\Omega}, \ \boldsymbol{\mathcal{I}}[\boldsymbol{\Omega}] \right] = \boldsymbol{\Omega} \cdot \boldsymbol{\mathcal{I}}[\boldsymbol{\Omega}] - \boldsymbol{\mathcal{I}}[\boldsymbol{\Omega}] \cdot \boldsymbol{\Omega}$ 是矩阵交换子（李括号），代表高维旋转中的"陀螺进动项"。即使无外力矩，也会因质量分布不对称导致姿态进动。

### 2.4 完整状态演化方程组

一个 n 维刚体的完整运动状态，由 **位置 + 姿态** 及其一阶导数共同决定：

- **广义坐标**：$(\mathbf{x},\ R)$，其中 $\mathbf{x} \in \mathbb{R}^n$，$R \in SO(n)$。
- **广义速度**：$(\mathbf{v},\ \boldsymbol{\Omega})$，其中 $\mathbf{v} = \dfrac{d\mathbf{x}}{dt} \in \mathbb{R}^n$，$\boldsymbol{\Omega} = R^T \dfrac{dR}{dt} \in \mathfrak{so}(n)$。

状态演化方程组（封闭形式）：

$$
\begin{cases}
\dfrac{d\mathbf{x}}{dt} = \mathbf{v} \\[6pt]
\dfrac{d\mathbf{v}}{dt} = \dfrac{1}{m} \mathbf{F} \\[10pt]
\dfrac{dR}{dt} = R \cdot \boldsymbol{\Omega} \\[6pt]
\dfrac{d\boldsymbol{\Omega}}{dt} = \boldsymbol{\mathcal{I}}^{-1} \left[ \boldsymbol{\tau} - \left[ \boldsymbol{\Omega}, \ \boldsymbol{\mathcal{I}}[\boldsymbol{\Omega}] \right] \right]
\end{cases}
$$


### 2.5 低维退化（$n = 3$ 时的检验）

当 $n=3$ 时，$SO(3)$ 即为三维旋转群：

- 旋转矩阵 $R$ 是 $3 \times 3$ 正交矩阵。
- 反对称矩阵 $\boldsymbol{\Omega}$ 可通过霍奇对偶映射为轴矢量 $\vec{\omega}$。
- 方程 $\dfrac{dR}{dt} = R \boldsymbol{\Omega}$ 退化为旋转矩阵的微分方程。
- 欧拉方程退化为经典的矢量形式：

  $$ \boldsymbol{\mathcal{I}} \cdot \dfrac{d\vec{\omega}}{dt} + \vec{\omega} \times (\boldsymbol{\mathcal{I}} \cdot \vec{\omega}) = \vec{\tau} $$

因此，这套高维方案完美兼容三维经典刚体力学。

---

## 3. 离散时间更新（$dt$ 内的运动积分）

### 3.1 前提假设与初始条件

- 时间间隔为极短增量 $dt$（即 $t = 0 \to t = dt$）。
- 在 $[0, dt]$ 内，**平动加速度 $\mathbf{a}_0$** 和 **角加速度 $\boldsymbol{\alpha}_0$** 保持恒定（均为常数）。
- 使用体坐标系下的角速度定义，即姿态矩阵满足 $\dfrac{dR}{dt} = R \cdot \boldsymbol{\Omega}$，其中 $\boldsymbol{\Omega} \in \mathfrak{so}(n)$。

**初始条件（$t=0$ 时刻）：**

| 物理量 | 符号 |
| :--- | :--- |
| 位置 | $\mathbf{x}_0$ |
| 速度 | $\mathbf{v}_0$ |
| 加速度 | $\mathbf{a}_0$（常数） |
| 姿态（旋转矩阵） | $R_0 \in SO(n)$ |
| 角速度（反对称矩阵） | $\boldsymbol{\Omega}_0 \in \mathfrak{so}(n)$ |
| 角加速度（反对称矩阵） | $\boldsymbol{\alpha}_0 \in \mathfrak{so}(n)$（常数） |

### 3.2 平动更新

由于 $\mathbf{a}_0$ 恒定，直接套用经典牛顿运动学公式：

**速度更新：**
$$
\mathbf{v}(dt) = \mathbf{v}_0 + \mathbf{a}_0 \cdot dt
$$

**位置更新：**
$$
\mathbf{x}(dt) = \mathbf{x}_0 + \mathbf{v}_0 \cdot dt + \frac{1}{2} \mathbf{a}_0 \cdot (dt)^2
$$

### 3.3 转动更新

#### 角速度更新

由于 $\boldsymbol{\alpha}_0$ 恒定，角速度随时间线性变化：

$$ \boldsymbol{\Omega}(dt) = \boldsymbol{\Omega}_0 + \boldsymbol{\alpha}_0 \cdot dt $$

#### 姿态更新

平动中位置是速度的积分（$\mathbf{x} = \int \mathbf{v} \, dt$）。**但在旋转中，姿态 $R$ 不能简单地直接积分角速度**，因为不同时刻的角速度矩阵 $\boldsymbol{\Omega}(t)$ 互不对易（$[\boldsymbol{\Omega}(t_1), \boldsymbol{\Omega}(t_2)] \neq 0$）。

因此，姿态的更新必须通过求解矩阵微分方程 $\dfrac{dR}{dt} = R \boldsymbol{\Omega}$ 得到。

**(a) 精确到 $dt^2$ 阶的泰勒展开式**

令相对旋转增量 $\Delta R(dt) = R_0^{-1} R(dt)$，则 $\Delta R(0) = I$。对 $\Delta R(dt)$ 在 $dt=0$ 处泰勒展开：

$$
\Delta R(dt) = I + \dfrac{d(\Delta R)}{dt}(0) \cdot dt + \frac{1}{2} \dfrac{d^{2}(\Delta R)}{dt^{2}}(0) \cdot (dt)^2 + O(dt^3)
$$

计算各阶导数：
- $\dfrac{d(\Delta R)}{dt}(0) = \boldsymbol{\Omega}_0$
- $\dfrac{d^{2}(\Delta R)}{dt^{2}}(0) = \boldsymbol{\Omega}_0^2 + \boldsymbol{\alpha}_0$

代入可得：

$$
\Delta R(dt) = I + \boldsymbol{\Omega}_0 \cdot dt + \frac{1}{2} \left( \boldsymbol{\Omega}_0^2 + \boldsymbol{\alpha}_0 \right) \cdot (dt)^2 + O(dt^3)
$$

因此姿态更新为：

$$
R(dt) = R_0 \cdot \left[ I + \boldsymbol{\Omega}_0 \, dt + \frac{1}{2} \left( \boldsymbol{\Omega}_0^2 + \boldsymbol{\alpha}_0 \right) (dt)^2 \right] + O(dt^3)
$$

> **注意**：这里的矩阵乘法顺序不可交换，必须保持 $R_0$ 左乘该括号。

**(b) 保结构数值更新（指数映射形式，工程推荐）**

为了在数值积分中自动保证 $R(dt) \in SO(n)$（即保持正交性且行列式为 $+1$），通常使用 **李代数到李群的指数映射**。

因为在 $dt$ 内角速度线性变化，其积分效应（旋转矢量）为：

$$
\Theta = \int_0^{dt} \boldsymbol{\Omega}(t) \, dt = \boldsymbol{\Omega}_0 \cdot dt + \frac{1}{2} \boldsymbol{\alpha}_0 \cdot (dt)^2
$$

由于 $\Theta \in \mathfrak{so}(n)$（反对称矩阵），其指数映射 $\exp(\Theta) \in SO(n)$。忽略 $O(dt^3)$ 的非对易误差，姿态更新可写为：

$$
R(dt) = R_0 \cdot \exp\!\left( \boldsymbol{\Omega}_0 \cdot dt + \frac{1}{2} \boldsymbol{\alpha}_0 \cdot (dt)^2 \right) + O(dt^3)
$$

将指数映射展开（$\exp(X) = I + X + \frac{1}{2}X^2 + \cdots$），其 $dt^2$ 项与上述泰勒展开完全一致，因此二者在 $O(dt^3)$ 精度内等效。但指数映射形式能确保数值稳定性。

### 3.4 最终汇总表（$t = dt$ 时刻）

| 物理量                               | 更新表达式                                                                                                                        | 备注                        |
| :-------------------------------- | :--------------------------------------------------------------------------------------------------------------------------- | :------------------------ |
| **位置** $\mathbf{x}(dt)$           | $\mathbf{x}_0 + \mathbf{v}_0 dt + \frac{1}{2} \mathbf{a}_0 (dt)^2$                                                           | 精确（加速度恒定）                 |
| **速度** $\mathbf{v}(dt)$           | $\mathbf{v}_0 + \mathbf{a}_0 dt$                                                                                             | 精确                        |
| **角速度** $\boldsymbol{\Omega}(dt)$ | $\boldsymbol{\Omega}_0 + \boldsymbol{\alpha}_0 dt$                                                                           | 精确                        |
| **姿态** $R(dt)$                    | $R_0 \cdot \exp\!\left( \boldsymbol{\Omega}_0 dt + \frac{1}{2} \boldsymbol{\alpha}_0 (dt)^2 \right)$                         | 二阶精度（忽略 $O(dt^3)$ 高阶非对易项） |
| **姿态** $R(dt)$（展开形式）              | $R_0 \cdot \left[ I + \boldsymbol{\Omega}_0 dt + \frac{1}{2}(\boldsymbol{\Omega}_0^2 + \boldsymbol{\alpha}_0)(dt)^2 \right]$ | 显式二阶近似                    |

### 3.5 关键要点

1. **位置与姿态的解耦**：平动仅由 $\mathbf{F}$ 决定，转动仅由 $\boldsymbol{\tau}$ 决定。在 $dt$ 极短时，加速度和角加速度恒定是合理的数值积分假设（如 Velocity Verlet 或李群积分子步）。
2. **不可对易性**：如果 $dt$ 较大，不能直接写作 $R(dt) = R_0 \exp(\Omega_0 dt) \exp(\frac{1}{2}\alpha_0 dt^2)$，因为矩阵指数之间需要贝克-坎贝尔-豪斯多夫（BCH）公式修正。但在 $dt$ 极短的情况下，上述表达式给出的 $O(dt^3)$ 误差完全可接受。
3. **三维退化**：当 $n=3$ 时，上述公式退化为经典的角速度矢量更新，且 $\Omega_0^2$ 对应向量的张量积项，完全兼容标准的刚体旋转运动学。
