# 碰撞图算法：接触岛与同步位置补偿

<!--
/**
 * @file Chapter-5-ContactGraph.md
 * @brief 项目现有碰撞图求解器的数学模型、数值流程与适用边界。
 * @details 以 ContactGraphProjector、接触解算策略和 Solver 当前实现为依据，
 *          接续支撑函数、GJK、EPA、刚体运动及 CCD 章节。
 * @date 2026-10-17
 */
-->

## 1. 问题陈述与算法目标

### 1.1 从碰撞检测到碰撞响应

前述章节依次给出了凸体的几何表示、静态碰撞判定、接触信息生成和连续碰撞时间计算：

| 阶段 | 输入 | 主要输出 |
| :--- | :--- | :--- |
| [[Chapter-0-支撑函数与支撑点]] | 凸体与搜索方向 | 支撑点 |
| [[Chapter-1-GJK算法：静态碰撞检测]] | 两个凸体的支撑映射 | 碰撞判定、最近点及单纯形 |
| [[Chapter-2-EPA算法：静态碰撞信息生成]] | 相交凸体与 GJK 单纯形 | 法线、穿透深度与接触点 |
| [[Chapter-3-牛顿定律推广：高维刚体运动模型]] | 刚体状态与受力 | 位置、姿态和速度的演化 |
| [[Chapter-4-CCD算法：连续碰撞检测]] | 运动中的两个凸体 | 碰撞时间与该时刻的接触信息 |

上述信息主要针对一对物体。实际场景中，一个刚体可以同时接触多个其它刚体，多组接触还可能形成闭环。此时，单独知道每一对物体应当分离多少，并不足以确定整个系统应当如何移动。

**碰撞图求解**的目标是：在同一物理时刻收集相互耦合的接触约束，计算一组共同满足这些约束的速度变化与位姿修正，避免逐对位置补偿对后续碰撞对产生未经统一分析的影响。

> **实现范围**：本章以项目当前的三维刚体实现为准。支撑函数、GJK、EPA 和 CCD 的前置章节讨论了维度推广，但当前碰撞图求解器使用三维叉积、六维刚体增量和三维四元数，不能直接据此认定其已经支持任意维数。

### 1.2 链式位置补偿的问题

考虑三个动态刚体组成的闭环：

```text
       A
      / \
     /   \
    B─────C
```

若按照 $A\!B\rightarrow B\!C\rightarrow C\!A$ 的顺序逐对推开物体，则：

1. 修正 $A\!B$ 时，改变了 $A$ 和 $B$ 的状态。
2. 修正 $B\!C$ 时，使用已经改变的 $B$，并可能重新破坏 $A\!B$。
3. 修正 $C\!A$ 时，可能再次破坏前面两条边。

有限次迭代后，结果通常与遍历顺序有关。如果还重复使用最初检测得到的穿透深度，已经消除的穿透可能被重复补偿。

本章采用的处理方式是：在同一份位姿快照上构建整岛约束系统，统一计算所有刚体的修正量，再提交位姿变化。**同步**描述的是求解与写回的语义，不要求多个线程在同一机器指令时刻写入状态。

### 1.3 算法设计目标

| 目标 | 数学或工程含义 |
| :--- | :--- |
| **保留完整耦合** | 闭环、多连接节点和同一实体对的多个接触点均进入系统 |
| **统一位置补偿** | 所有增量根据同一轮快照计算，不按接触边立即写回 |
| **按质量与惯性分配** | 使用刚体逆质量、世界逆惯性构建修正度量 |
| **允许单边接触** | 接触可以分离，法向接触乘子不能变为吸引作用 |
| **处理冗余约束** | 不要求接触矩阵可逆，不以删除闭环换取可解性 |
| **约束误差可检查** | 给出内迭代残差、外迭代误差与预算耗尽条件 |
| **区分物理耗散与数值误差** | 摩擦、阻力、恢复系数和位置投影分别说明 |

---

## 2. 基本概念与符号约定

### 2.1 刚体状态

对于三维刚体 $i$，使用下列符号：

| 符号 | 含义 | 数学空间 |
| :--- | :--- | :--- |
| $\mathbf{x}_i$ | 质心位置 | $\mathbb{R}^3$ |
| $R_i$ | 从刚体坐标系到世界坐标系的姿态 | $SO(3)$ |
| $\mathbf{v}_i$ | 质心线速度 | $\mathbb{R}^3$ |
| $\boldsymbol{\omega}_i$ | 世界坐标系角速度 | $\mathbb{R}^3$ |
| $m_i$ | 质量 | 动态刚体为正数 |
| $I_i$ | 世界坐标系转动惯性矩阵 | $\mathbb{R}^{3\times3}$ |
| $\delta\mathbf{x}_i$ | 一轮位置修正中的平移增量 | $\mathbb{R}^3$ |
| $\delta\boldsymbol{\theta}_i$ | 一轮位置修正中的世界转角增量 | $\mathbb{R}^3$ |

定义单个刚体的广义位姿增量和广义速度：

$$
\boldsymbol{\xi}_i=
\begin{bmatrix}
\delta\mathbf{x}_i\\
\delta\boldsymbol{\theta}_i
\end{bmatrix},\qquad
\mathbf{u}_i=
\begin{bmatrix}
\mathbf{v}_i\\
\boldsymbol{\omega}_i
\end{bmatrix}.
$$

对于包含 $N$ 个动态刚体的求解岛，将这些向量按刚体排列堆叠，得到 $\boldsymbol{\xi},\mathbf{u}\in\mathbb{R}^{6N}$。

### 2.2 接触法线与有符号间隙

对于接触 $c=(A,B)$，统一规定单位法线 $\mathbf{n}_c$ 从 $A$ 指向 $B$。施加正法向冲量时，$A$ 接收 $-\mathbf{n}_c$ 方向的冲量，$B$ 接收 $+\mathbf{n}_c$ 方向的冲量。

令 $\mathbf{p}_A,\mathbf{p}_B$ 为双方的接触锚点，定义有符号间隙：

$$
C_c=\mathbf{n}_c^T(\mathbf{p}_B-\mathbf{p}_A).
$$

其符号含义为：

$$
\begin{cases}
C_c>0,&\text{分离；}\\
C_c=0,&\text{接触；}\\
C_c<0,&\text{穿透。}
\end{cases}
$$

> **法线约定**：EPA 在相交状态下给出的闵可夫斯基边界法线，与分离状态下 GJK 最近差向量的方向含义不同。进入响应层前必须统一到上述约定，不能仅依据向量名称判断方向，也不能将偏心平面接触的质心连线当作几何法线。

### 2.3 接触点与接触流形

一个接触点对应一条法向标量约束。多个接触点组成一个**接触流形**：

$$
\mathcal{M}_{AB}=\{c_1,c_2,\ldots,c_k\}.
$$

平面对平面的支撑通常需要多个分布在接触区域上的点，以共同约束平移和转动。当前实现对盒体面采用参考面裁剪，对平行圆柱端面采集端面接触区域；其它形状还可以通过共面支撑特征采样形成候选点。无法形成有效面特征时，保留原 CCD 接触结果。

流形内的多个点并不是可以任意合并的一条标量约束。它们具有不同的力臂，对角速度和转角的影响也不同。

---

## 3. 碰撞图与接触岛

### 3.1 图的定义

定义动态刚体的接触多重图：

$$
\mathcal{G}=(\mathcal{V},\mathcal{E}).
$$

其中，$\mathcal{V}$ 为当前具有可变自由度的刚体集合，$\mathcal{E}$ 为动态刚体之间的接触和已接入的位置约束关系。同一实体对可以对应多个接触点，因此允许平行边。

图只记录“哪些未知量相互耦合”。真正决定修正方向、强度和冗余性的，是后续构造的约束雅可比和逆质量矩阵。

**闭环不等于数值奇异。** 一个图中存在环，不代表其约束行必然线性相关；没有实体层闭环，也可能因为共面重复接触而出现相关行。

### 3.2 静态边界

静态实体的逆质量和逆惯性设为零。它们提供边界约束，但不作为合并独立动态岛的桥梁：

```text
     A              B
     │              │
  ───┴──────────────┴─── 固定地面

  动态岛 1：{A}     动态岛 2：{B}
```

如果 $A$ 与 $B$ 没有其它动态连接，则即使接触同一地面，它们也不通过地面位移发生耦合，应当分别求解。

当前实现按照求解句柄的 `inverseMass > 0` 判断是否纳入动态未知量。休眠实体可能暂时表现为零逆质量边界；外部冲击唤醒后，其质量和惯性重新参与求解。

### 3.3 接触岛划分

对每条连接两个动态刚体的约束边执行并查集合并；对于连接动态体和静态边界的约束，只将该行归入动态体所属的岛。

一座**接触岛**是共享动态未知量的约束集合。不同岛的位置系统可以独立求解。

当前实现保留岛中的全部接触行，不提取生成树、不删除闭环，也不对高连接节点简单取位移平均值。

---

## 4. 接触锚点与约束线性化

### 4.1 当前实现中的锚点构造

响应层输入包含接触中点 $\mathbf{c}$、单位法线 $\mathbf{n}$ 和穿透深度 $\delta\ge0$。当前位置投影器据此构造一对等效初始锚点：

$$
\mathbf{p}_A^0=\mathbf{c}+\frac{\delta}{2}\mathbf{n},\qquad
\mathbf{p}_B^0=\mathbf{c}-\frac{\delta}{2}\mathbf{n}.
$$

于是初始间隙恰好为 $C^0=-\delta$。将锚点存储到双方各自的刚体坐标系：

$$
\mathbf{a}=R_A^T(\mathbf{p}_A^0-\mathbf{x}_A),\qquad
\mathbf{b}=R_B^T(\mathbf{p}_B^0-\mathbf{x}_B).
$$

后续外迭代根据当前姿态恢复世界锚点：

$$
\mathbf{r}_A=R_A\mathbf{a},\qquad
\mathbf{r}_B=R_B\mathbf{b},
$$

$$
C=\mathbf{n}^T\left(\mathbf{x}_B+\mathbf{r}_B-\mathbf{x}_A-\mathbf{r}_A\right).
$$

这样，已经施加的平移和转角会反映到下一轮间隙中，而不会在每轮重复补偿原始 $\delta$。

> **几何近似**：这对锚点是由中点和深度重建的等效点，不保证对任意接触几何都恰好落在双方真实表面。当前一次 `project()` 调用内固定接触法线、更新旋转锚点；完整窄相位和新接触构造仍由外部求解调度器执行。

### 4.2 单条接触的雅可比

对于世界坐标系小转角 $\delta\boldsymbol{\theta}$，锚点变化满足：

$$
\delta\mathbf{r}=\delta\boldsymbol{\theta}\times\mathbf{r}
+O\!\left(\|\delta\boldsymbol{\theta}\|^2\right).
$$

因此，单条间隙的一阶变化为：

$$
\begin{aligned}
\delta C
={}&-\mathbf{n}^T\delta\mathbf{x}_A
-(\mathbf{r}_A\times\mathbf{n})^T\delta\boldsymbol{\theta}_A\\
&+\mathbf{n}^T\delta\mathbf{x}_B
+(\mathbf{r}_B\times\mathbf{n})^T\delta\boldsymbol{\theta}_B.
\end{aligned}
$$

定义接触雅可比行：

$$
J_c=
\begin{bmatrix}
-\mathbf{n}^T&-(\mathbf{r}_A\times\mathbf{n})^T&
+\mathbf{n}^T&+(\mathbf{r}_B\times\mathbf{n})^T
\end{bmatrix}.
$$

将该行填入双方刚体对应的列，其它刚体列为零，得到：

$$
C_c(q\oplus\boldsymbol{\xi})
\approx C_c(q)+J_c\boldsymbol{\xi}.
$$

其中，$q\oplus\boldsymbol{\xi}$ 表示对位置作加法、对姿态作旋转复合，而非直接相加旋转矩阵。

### 4.3 杆与绳的位置约束

当前项目的连接杆和连接线均以质心间距为参考量。设参考长度为 $\ell$，定义：

$$
C_\ell=\ell-\|\mathbf{x}_B-\mathbf{x}_A\|.
$$

当距离非零时，取：

$$
\mathbf{n}_\ell=-\frac{\mathbf{x}_B-\mathbf{x}_A}
{\|\mathbf{x}_B-\mathbf{x}_A\|},\qquad
J_\ell=\begin{bmatrix}-\mathbf{n}_\ell^T&\mathbf{0}^T&
+\mathbf{n}_\ell^T&\mathbf{0}^T\end{bmatrix}.
$$

连接杆要求 $C_\ell=0$，是**双边约束**；连接线要求 $C_\ell\ge0$，是只能拉紧、允许松弛的**单边约束**。

距离方向在外迭代中重新计算。两质心完全重合时方向不唯一，当前实现保留预设方向作为退化处理，不能将其解释为唯一物理解。

---

## 5. 质量与惯性加权

### 5.1 广义逆质量矩阵

在非奇异情况下，单个动态刚体的质量矩阵及其逆为：

$$
M_i=\begin{bmatrix}m_iI_3&0\\0&I_i\end{bmatrix},\qquad
W_i=M_i^{-1}=\begin{bmatrix}m_i^{-1}I_3&0\\0&I_i^{-1}\end{bmatrix}.
$$

全岛矩阵按刚体组成块对角结构：

$$
W=\operatorname{diag}(W_1,W_2,\ldots,W_N).
$$

静态实体对应的块为零，或直接从未知量中删除。没有转动自由度的质点，其角向块也为零。

### 5.2 奇异惯性的处理

细棒等理想化几何可能具有零主惯量。当前实现对世界惯性作对称特征分解：

$$
I=Q\operatorname{diag}(\eta_1,\eta_2,\eta_3)Q^T,
$$

并将高于截断阈值的正特征值取倒数，其余取零，构造逆惯性算子。该处理将零逆惯性方向视为不可由当前冲量更新的转动自由度，而非使用无穷大的角加速度。

此时 $W\succeq0$。下节使用 $M=W^{-1}$ 的原始问题推导应理解为在可变子空间上的表达；更一般地，可写成以 $W^\dagger$ 为度量、并要求 $\boldsymbol{\xi}\in\operatorname{Range}(W)$ 的问题。实际数值迭代直接使用 $W$，不需要求其普通逆矩阵。

---

## 6. 整岛位置补偿的优化模型

### 6.1 目标修正量

设允许的穿透容差为 $s\ge0$，位置恢复比例为 $\beta\in[0,1]$。对于接触或连接线的单边行，当前实现定义：

$$
b_c=\beta\max(-C_c-s,0).
$$

对于连接杆的双边行，定义：

$$
b_c=-\beta C_c.
$$

单边约束的目标为 $J_c\boldsymbol{\xi}\ge b_c$，双边约束的目标为 $J_c\boldsymbol{\xi}=b_c$。

**当前策略的含义**：即使某条单边行尚未穿透，$b_c=0$ 仍要求这一轮线性增量不继续关闭该间隙。这比仅要求最终间隙非负更保守；它也会限制已有松绳在当前投影轮次中的进一步伸长。只有已经收集到求解系统的行才具有该约束作用。

### 6.2 原始问题

令 $\mathcal{U}$、$\mathcal{B}$ 分别为单边行与双边行的索引集合。在可变子空间上，求解：

$$
\begin{aligned}
\min_{\boldsymbol{\xi}}\quad&
\frac12\boldsymbol{\xi}^TM\boldsymbol{\xi}\\
\text{s.t.}\quad&J_c\boldsymbol{\xi}\ge b_c,\quad c\in\mathcal{U},\\
&J_c\boldsymbol{\xi}=b_c,\quad c\in\mathcal{B}.
\end{aligned}
$$

该目标寻找满足本轮约束的**质量与惯性加权最小修正**。这里的二次型是位置优化度量；平移项和转角项经惯性加权具有一致量纲，但它本身不是刚体真实动能。

### 6.3 对偶问题推导

对单边约束 $\mathbf{b}-J\boldsymbol{\xi}\le0$ 引入非负乘子，对双边约束引入自由乘子，统一记为 $\boldsymbol{\lambda}$。拉格朗日函数为：

$$
\mathcal{L}(\boldsymbol{\xi},\boldsymbol{\lambda})=
\frac12\boldsymbol{\xi}^TM\boldsymbol{\xi}
+\boldsymbol{\lambda}^T(\mathbf{b}-J\boldsymbol{\xi}).
$$

对 $\boldsymbol{\xi}$ 求驻点：

$$
M\boldsymbol{\xi}-J^T\boldsymbol{\lambda}=0
\quad\Longrightarrow\quad
\boldsymbol{\xi}=WJ^T\boldsymbol{\lambda}.
$$

代回后，将对偶最大化转为最小化，得到：

$$
\min_{\boldsymbol{\lambda}\in\mathcal{K}}
f(\boldsymbol{\lambda})=
\frac12\boldsymbol{\lambda}^TA\boldsymbol{\lambda}
-\mathbf{b}^T\boldsymbol{\lambda},\qquad
A=JWJ^T,
$$

其中乘子可行集合为：

$$
\mathcal{K}=\left\{\boldsymbol{\lambda}\;\middle|\;
\lambda_c\ge0\ (c\in\mathcal{U}),\quad
\lambda_c\in\mathbb{R}\ (c\in\mathcal{B})\right\}.
$$

由于 $W\succeq0$，对任意向量 $\mathbf{z}$ 有：

$$
\mathbf{z}^TA\mathbf{z}
=(J^T\mathbf{z})^TW(J^T\mathbf{z})\ge0.
$$

因此 $A$ 为对称半正定矩阵，对偶目标是凸二次函数。

### 6.4 KKT 条件

定义线性约束余量：

$$
\mathbf{w}=A\boldsymbol{\lambda}-\mathbf{b}.
$$

对于单边行，最优解满足：

$$
\lambda_c\ge0,\qquad w_c\ge0,\qquad\lambda_cw_c=0.
$$

对于双边行，最优解满足 $w_c=0$，乘子可以为正或负。

这些条件表达了单边约束的互补关系：没有作用的位置乘子允许约束留有余量；施加正乘子的行在理想线性解中达到本轮目标。

### 6.5 图耦合如何进入矩阵

矩阵元素可以写成：

$$
A_{cd}=\sum_i J_{c,i}W_iJ_{d,i}^T.
$$

若约束 $c$、$d$ 不共享任何可变刚体，则 $A_{cd}=0$；若共享刚体，则通常出现非零耦合项。多连接节点会让多条约束通过同一个 $W_i$ 发生联系，闭环则通过完整雅可比共同进入系统。

冗余接触可能使 $A$ 奇异。对偶乘子此时可能不唯一，但在原始问题可行、质量度量在可变子空间正定时，最小位姿修正仍唯一。当前方法不通过计算 $A^{-1}$ 求解，因此无需为了可逆性删除闭环或任意约束行。

---

## 7. 数值求解：矩阵自由投影梯度

### 7.1 同步乘子更新

目标梯度为：

$$
\nabla f(\boldsymbol{\lambda})=A\boldsymbol{\lambda}-\mathbf{b}.
$$

当前实现从 $\boldsymbol{\lambda}^{(0)}=0$ 开始，执行：

$$
\boldsymbol{\lambda}^{(k+1)}=
\Pi_{\mathcal{K}}\left[
\boldsymbol{\lambda}^{(k)}+
\frac{\mathbf{b}-A\boldsymbol{\lambda}^{(k)}}{L}
\right].
$$

投影 $\Pi_{\mathcal{K}}$ 对单边分量取 $\max(\lambda_c,0)$，对双边分量保持不变。**所有分量使用同一个旧乘子向量计算**，不在计算下一行时提前使用刚刚更新的分量。

### 7.2 安全步长上界

对梯度采用固定步长时，需要控制 $A$ 的最大特征值。当前实现使用：

$$
L=\left\|\,|J|\,|W|\,|J|^T\mathbf{1}\,\right\|_\infty,
$$

其中绝对值逐元素计算。由逐元素不等式：

$$
|JWJ^T|\le |J|\,|W|\,|J|^T
$$

可得：

$$
\lambda_{\max}(A)\le\|A\|_\infty\le L.
$$

所以在有限、有效的 $L>0$ 下，$1/L$ 是凸二次投影梯度的保守步长。共享刚体越多，上界可能越保守，收敛速度仍受条件数影响。

> **收敛条件**：上述结论针对固定线性系统、存在最优解且允许足够迭代的情形。固定 256 次迭代不等于任意碰撞图都达到精确解；原始约束不可行时，也不能用这个结论保证找到可行位姿。

### 7.3 矩阵自由乘法

实现中不显式构造可能发生大量填充的 $A$，而按下列顺序计算：

$$
\mathbf{z}=J^T\boldsymbol{\lambda},\qquad
\mathbf{y}=W\mathbf{z},\qquad
A\boldsymbol{\lambda}=J\mathbf{y}.
$$

$J$ 和 $W$ 以稀疏矩阵存储。每轮乘法成本与 $\operatorname{nnz}(J)+\operatorname{nnz}(W)$ 同阶；总成本还取决于迭代次数、外迭代、岛构造和碰撞检测，不能据此宣称整个引擎为线性复杂度。

### 7.4 内迭代终止

当前位置求解采用投影梯度映射残差：

$$
\rho_k=L\left\|
\boldsymbol{\lambda}^{(k+1)}-\boldsymbol{\lambda}^{(k)}
\right\|_\infty.
$$

当 $\rho_k\le\varepsilon_p$，或达到最大内迭代数时结束。残差为零对应乘子投影的固定点，但它不是最终真实形状的最大穿透，也不能代替外部几何检查。

得到乘子后，统一计算：

$$
\boldsymbol{\xi}=WJ^T\boldsymbol{\lambda}.
$$

---

## 8. 非线性外迭代与同步提交

### 8.1 外迭代的必要性

雅可比采用当前姿态下的一阶近似，而旋转属于非线性运动。因此，一次线性优化后需要更新锚点和世界逆惯性，再计算剩余约束误差。

当前每座岛执行以下外迭代：

1. 从当前位姿快照恢复双方锚点，计算间隙与距离约束方向。
2. 构造 $J$、$W$ 和本轮目标 $\mathbf{b}$。
3. 对固定系统执行第 7 节的内迭代。
4. 计算全岛位姿增量，并选择共同缩放系数 $\alpha\in[0,1]$。
5. 在试探位姿上评估锚点约束误差，必要时回溯 $\alpha$。
6. 接受后提交所有刚体位姿，再进入下一轮。

外迭代开始时，如果最大单边违反量及杆的绝对长度误差已不超过 $s+\varepsilon_p$，则提前结束。当前这项停止检查也使用了公共 $s$，因此双边长度并非始终追求零误差。

### 8.2 整岛共同步长

设本次投影调用中刚体 $i$ 的累计平移修正为 $\mathbf{d}_i$，平移预算为 $d_{\max}$。代码保守地使用剩余量：

$$
B_i=\max(d_{\max}-\|\mathbf{d}_i\|,0)
$$

并限制 $\alpha\|\delta\mathbf{x}_i\|\le B_i$。角向也根据累计角度预算限制整岛共同的 $\alpha$。

如果分别截断各个刚体的增量，就会改变 $WJ^T\boldsymbol{\lambda}$ 所描述的相互分配关系。使用共同缩放可以保持本轮增量方向以及内部成对平移的平衡。

> **预算作用范围**：参数虽然命名为 `maxPositionCorrectionPerStep` 和 `maxAngularCorrectionPerStep`，目前累计器实际位于单次 `ContactGraphProjector::project()` 调用内。同一个固定步中若多次调用投影器，尚未共享一个跨调用总预算。

### 8.3 误差函数与回溯

对试探位姿 $q(\alpha)$，定义：

$$
E(\alpha)=
\sum_{c\in\mathcal{U}}\min(C_c(q(\alpha))+s,0)^2
+\sum_{c\in\mathcal{B}}C_c(q(\alpha))^2.
$$

试探姿态使用旋转复合，因此这里更新了旋转锚点，而非只计算 $C+J\boldsymbol{\xi}$。但接触法线和锚点特征仍属于当前调用固定的几何近似。

当前最多尝试 12 次：若 $E(\alpha)\le E(0)+10^{-20}$ 且结果有限，则接受；否则令 $\alpha\leftarrow\alpha/2$。若没有接受的试探，结束该轮并记录未收敛状态。

### 8.4 位姿更新

接受后，对所有动态刚体统一提交：

$$
\mathbf{x}_i'=\mathbf{x}_i+\alpha\delta\mathbf{x}_i,
$$

$$
R_i'=\exp\!\left([\alpha\delta\boldsymbol{\theta}_i]_\times\right)R_i.
$$

其中 $[\mathbf{a}]_\times\mathbf{b}=\mathbf{a}\times\mathbf{b}$。代码通过 `Eigen::AngleAxisd` 和四元数复合实现该三维指数映射。

对于发生转角修正的刚体，逆惯性随世界姿态更新。当前还根据修正前角速度恢复自旋角动量，再用新逆惯性重建角速度。非奇异情况下为：

$$
\mathbf{L}_i=I_i\boldsymbol{\omega}_i,\qquad
\boldsymbol{\omega}_i'=I_i'^{-1}\mathbf{L}_i.
$$

奇异情况下使用伪逆算子，其物理解释限于可达转动子空间。

---

## 9. 速度层：整体法向冲量与摩擦

### 9.1 位置乘子与速度冲量的区别

位置层与速度层使用相同形式的接触雅可比，但求解变量的量纲和目标不同。位置乘子用于构造位姿修正；速度层乘子表示冲量，作用于广义速度：

$$
\mathbf{u}'=\mathbf{u}+WJ^T\boldsymbol{\lambda}_n.
$$

位置恢复不直接作为额外的恢复速度反复叠加到冲击中。

### 9.2 固定恢复目标

在本次接触解算开始、应用初始冲量之前，记录各接触点的法向相对速度 $v_{n,c}^{\mathrm{pre}}$。对于恢复系数 $e_c$ 和速度门限 $v_{\mathrm{th}}$，设置：

$$
v_{n,c}^{\mathrm{target}}=
\begin{cases}
-e_c v_{n,c}^{\mathrm{pre}},&v_{n,c}^{\mathrm{pre}}<-v_{\mathrm{th}},\\
0,&\text{其它情形。}
\end{cases}
$$

恢复目标在本次迭代中保持不变。门限的作用是关闭低速接触的微小弹跳；它具有速度量纲，需要与场景尺度匹配。

### 9.3 整体法向速度系统

定义：

$$
\mathbf{b}_v=\mathbf{v}_n^{\mathrm{target}}-J\mathbf{u}.
$$

法向冲量首先通过以下凸二次问题整体求解：

$$
\min_{\boldsymbol{\lambda}_n\ge0}
\frac12\boldsymbol{\lambda}_n^TJWJ^T\boldsymbol{\lambda}_n
-\mathbf{b}_v^T\boldsymbol{\lambda}_n.
$$

当前 `solveNormalGraph()` 使用与位置层相同形式的矩阵自由投影梯度，统一处理接触法向速度。该函数在接触数组上构造系统；独立岛对应相互解耦的块，不另外调用位置层的并查集。

### 9.4 累积冲量与切向摩擦

整体法向求解之后，继续迭代耦合法向和切向冲量。单个接触的法向增量为：

$$
\Delta\lambda_n=
\frac{v_n^{\mathrm{target}}-v_n}{J_nWJ_n^T},\qquad
\lambda_n'=\max(0,\lambda_n+\Delta\lambda_n).
$$

只施加 $\lambda_n'-\lambda_n$。该差值允许为负，用来撤回此前过大的累积冲量。

构造与 $\mathbf{n}$ 正交的切向基 $\mathbf{t}_1,\mathbf{t}_2$。令 $J_t$ 为两条切向雅可比，切向有效逆质量为：

$$
K_t=J_tWJ_t^T\in\mathbb{R}^{2\times2}.
$$

使用法向增量更新后的相对切向速度，求切向冲量候选，再投影到摩擦圆盘：

$$
\|\boldsymbol{\lambda}_t\|_2\le\mu\lambda_n.
$$

不能将两个切向分量分别裁剪后就称为圆锥投影：独立裁剪形成的正方形截面会允许合冲量超过 $\mu\lambda_n$。

> **同步范围**：当前位置补偿为整岛同步求解；法向速度先整体求解；后续摩擦耦合仍采用逐接触迭代。尚未将全部法向、摩擦和双边速度约束统一为一个同时冲击优化模型，因此不能声称整个速度求解已经完全消除顺序影响。

---

## 10. 与 CCD 时间推进的配合

碰撞图必须对应同一物理时刻。不能把整帧内不同时刻的接触信息直接拼接后，当成一组同时成立的约束。

当前 TOI 路径按以下流程调度：

1. 在当前状态下生成碰撞候选和 TOI 事件。现有空间索引之外，补充扫掠包围盒候选。
2. 推进到最早事件时刻，将时间相近的事件组成一批。
3. 在该时刻重新收集已有支撑接触，按实体对避免与事件流形重复加入，同时保留流形中的多个点。
4. 求解当前接触系统，并记录本步已处理的冲击实体对。
5. 根据已经改变的速度和位姿重新计算剩余区间的事件；本时刻已经解算的零 TOI 接触交给持续支撑阶段。
6. 完成剩余推进后，再收集步末接触，补齐受力积分产生的法向支撑速度。对本步已经处理的冲击对关闭重复恢复。

该路径仍保留恒速 CCD 预测和原有积分器，并没有将完整加速度轨迹直接纳入当前碰撞图求解。位置恢复负责处理残余穿透。

---

## 11. 数据处理与工程参数

### 11.1 输入数据

| 数据结构 | 主要内容 | 求解用途 |
| :--- | :--- | :--- |
| `SolverBodyHandle` | 实体指针、逆质量、世界逆惯性 | 构造动态未知量与 $W$ |
| `SolverContactConstraint` | 双方索引、中点、法线、深度、恢复及摩擦参数 | 构造接触行、恢复目标和冲量状态 |
| `CcdCollisionEvent` | TOI、双方索引、接触流形 | 对齐求解时刻 |
| `ConstraintsBase` | 已启用的位置约束及参考长度 | 接入杆和绳的距离行 |

数组索引只在当前场景快照内有效。当前实现没有持久接触缓存，不应把索引或重建的局部锚点误认为已经构成跨帧稳定特征匹配。

### 11.2 默认参数

| 参数 | 当前值 | 含义 |
| :--- | ---: | :--- |
| `positionIterations` | 2 | 每次位置投影的最大非线性外迭代数 |
| `graphIterations` | 256 | 单次固定线性系统的最大投影梯度迭代数 |
| `penetrationSlop` | 0.001 | 单边位置恢复容差，并参与外迭代提前停止 |
| `positionCorrectionBeta` | 0.2 | 本轮误差恢复比例 $\beta$ |
| `positionTolerance` | $10^{-7}$ | 位置内迭代映射残差阈值，也用于外迭代停止检查 |
| `maxPositionCorrectionPerStep` | 0.2 | 当前投影调用内的累计平移修正预算 |
| `maxAngularCorrectionPerStep` | 0.1 | 当前投影调用内的累计角度预算，单位为弧度 |
| `restitutionVelocityThreshold` | 0.5 | 关闭低速恢复的法向速度门限 |
| `velocityIterations` | 8 | 整体法向求解后的接触耦合迭代数 |

速度层整体法向求解内部另使用 $10^{-9}$ 的映射残差阈值。表中带量纲的数值按项目场景数值单位解释，不应在未经换算时当成任意尺度下的通用常量。

### 11.3 输出与诊断

位置投影最终更新刚体状态，并通过 `SolverStats` 返回：

| 字段 | 含义 |
| :--- | :--- |
| `contactIslandCount` | 本步各次调用累计求解的岛数量 |
| `positionGraphIterations` | 位置内迭代总次数 |
| `positionGraphResidual` | 各次位置内迭代结束时的最大映射残差 |
| `positionUnconvergedCount` | 内迭代未达容差、无效求解或回溯失败等记录次数 |
| `maxResidualPenetration` | 当前实现中的最大剩余约束违反量 |

最后一项名称虽然包含“穿透”，实际还可能汇总杆的长度误差，因此解释统计时需要区分几何类型。它也不是重新执行全部真实形状碰撞检测得到的全场景穿透上界。

---

## 12. 算法执行流程

以下伪代码描述当前位置投影器的主要流程：

```text
输入：刚体快照、当前接触、已启用的杆/绳约束、求解配置

构造等效局部接触锚点
加入杆的双边行与绳的单边行
按动态刚体连接关系划分接触岛，固定边界不用于合并

对每座岛：
    清零本次调用的累计平移与角度预算

    对每次非线性外迭代：
        根据同一位姿快照计算 C、J、W 和 b
        若约束误差已在容差内：结束该岛

        计算安全步长上界 L
        λ = 0
        对每次内迭代：
            y = J * (W * (Jᵀ * λ))
            λ_next = 投影到乘子可行集合(λ + (b - y) / L)
            计算投影梯度映射残差
            λ = λ_next
            若残差达标：结束内迭代

        ξ = W * (Jᵀ * λ)
        根据整岛预算限制共同缩放 α
        试探全部位置和姿态，回溯到约束误差不增大
        若试探失败或预算耗尽：记录状态并结束

        统一提交整岛的位置、姿态和角向状态

    统计剩余约束违反量

输出：更新后的刚体状态与位置图求解统计
```

关键函数对应关系：

| 实现位置 | 主要职责 |
| :--- | :--- |
| `src/model/solver/contact_manifold_builder.cpp` | 稳定法线与多点流形生成 |
| `src/model/solver/contact_graph_projector.cpp` | 分岛、位置约束优化、回溯与同步提交 |
| `src/model/strategy/contact_strategy/frictional_contact_resolver.cpp` | 整体法向速度求解及摩擦耦合 |
| `src/model/strategy/contact_strategy/frictionless_contact_resolver.cpp` | 将摩擦系数置零，复用相同求解路径 |
| `src/model/solver/solver.cpp` | TOI 调度、支撑补解、质量惯性缓存及唤醒 |

---

## 13. 能量、动量与数值边界

### 13.1 可以说明的性质

**内部平移的质量中心保持。** 对没有静态外部边界作用的内部成对约束，平移增量满足：

$$
\sum_i m_i\delta\mathbf{x}_i=0.
$$

原因是 $J^T\boldsymbol{\lambda}$ 在每对刚体的平移块中等大反向。整岛共同缩放后该关系仍成立；分别裁剪刚体增量一般会破坏它。

**自旋角动量的处理。** 对非奇异惯性，转角修正后重建角速度可保持单体的世界自旋角动量 $I\boldsymbol{\omega}$。这并不等于保持包含轨道项 $\mathbf{x}\times m\mathbf{v}$ 的全系统总角动量。

### 13.2 不能直接推出的性质

- 质量加权位置最小化不是机械能守恒条件；改变高度会改变势能。
- 姿态改变会改变世界惯性，即使保持自旋角动量，转动动能仍可能变化。
- 固定地面对动态物体施加外部作用，动态子系统的动量本来就不必守恒。
- 摩擦、空气阻力及 $e<1$ 的恢复模型具有物理耗散。
- 多体同时冲击即使各处 $e=1$，也不能仅靠逐点恢复目标推出全系统能量守恒。
- 接触顺序对精确凸投影解的影响可被消除，但有限迭代、浮点求和及后续摩擦迭代仍可能留下差异。

因此，本算法旨在减少可避免的重复补偿和顺序依赖，不提供任意场景中“零能量误差、零动量误差”的承诺。

### 13.3 当前实现限制

当前没有实现跨帧接触缓存、显式松弛变量、主动集分解或大型岛预条件器。不可行约束、极端质量比和高度病态的图可能在预算内无法收敛，需要结合残差判断。

杆、绳的位置行已经接入，部分其它旧约束仍通过独立接口执行；它们并非自动属于同一同步优化系统。新接触也不会在投影器每次试探内部重新运行完整窄相位，而是由后续调度发现。

扫掠候选补充目前采用两两包围盒检查，最坏复杂度为 $O(N^2)$。稀疏矩阵自由求解只解决数值系统的存储与乘法问题，不能消除这个检测阶段的代价。

---

## 14. 验证与参考资料

### 14.1 当前项目验证

自动回归包括三体闭环、重复边、约束反序、48 条边共享一个节点、静态边界不合岛、杆约束、多点平面支撑、摩擦圆锥和无休眠的弹跳/停稳仿真。它们是实现证据，不是所有图结构与接触模型的普遍证明。

具体构建命令、EPSX 记录和能量测量见[接触修复验证记录](../../../../ContactGraphValidation.md)；最初的方案与首版实现差异见[碰撞图设计方案](../../../../ContactGraphSolverDesign.md)。本章用于说明当前算法，实验参数及结果以对应验证记录为准。

### 14.2 理论与工程来源

本项目的方法属于标准约束雅可比、质量加权投影、凸二次规划和接触冲量方法的组合。文中针对现有代码给出了统一推导，不将该组合表述为具有专利新颖性的原创算法。

1. **Erin Catto，Modeling and Solving Constraints，GDC 2009。** 用于刚体约束雅可比、有效质量和接触响应的建模背景。[原始讲义](https://box2d.org/files/ErinCatto_ModelingAndSolvingConstraints_GDC2009.pdf)。
2. **Stephen Boyd、Lieven Vandenberghe，Convex Optimization。** 用于凸二次规划、拉格朗日对偶及 KKT 条件的理论背景。本章的矩阵自由步长界与公式按当前 $J,W$ 结构自行展开。[作者提供的教材主页](https://web.stanford.edu/~boyd/cvxbook/)。
3. **Erin Catto，Solver2D，2024。** 用于接触求解工程中的累积冲量回退、局部锚点和位置/速度阶段区分。其所述顺序求解器并不是本项目同步位置投影器的同一实现。[作者说明](https://box2d.org/posts/2024/02/solver2d/)。
4. **MuJoCo，Computation。** 全局接触优化的工程参照；其软接触模型与本章硬几何位置投影不同，不能视为等价复现。[官方文档](https://mujoco.readthedocs.io/en/stable/computation/)。
5. **Jolt Physics，ContactConstraintManager。** 多点流形与接触缓存的工程参照；本项目当前尚未实现该类跨帧缓存。[官方源码](https://github.com/jrouwe/JoltPhysics/blob/master/Jolt/Physics/Constraints/ContactConstraintManager.h)。

本章没有引用专利，也没有据此给出专利检索或自由实施结论。上述资料用于说明方法来源与相关技术，具体工程行为以本章对应的项目源代码为准。
