# GJK算法：静态碰撞检测

## 基本概念与定义

### 闵可夫斯基差
设 $A, B \subset \mathbb{R}^3$ 为非空集合，定义 $A$ 和 $B$ 的 **闵可夫斯基差** $A \ominus B$ 为：
$$
A \ominus B = \{ \mathbf{a} - \mathbf{b} \space | \space \mathbf{a} \in A, \mathbf{b} \in B \}
$$
这个集合包含了从 $B$ 集合中的每一个点到集合 $A$ 中的每一个点的所有差向量。

**关键性质** 原点 $\mathbf{O} \in A \ominus B \iff A \cap B \neq \varnothing$ 

### 单纯形
在 $n$ 维欧氏空间中，由 $k \in \mathbb{N}$ 且 $1 \leq k \leq n + 1$ 个仿射无关的点 $\mathbf{v}_0$，$\mathbf{v}_1$，$\mathbf{v}_{k-1}$ 构成的凸包被称为 一个 $k-1$ 维 **单纯形**：
$$
\sigma = \left\{ \sum_{i=0}^{k} \lambda_i \mathbf{v}_i \;\middle|\; \sum_{i=0}^{k} \lambda_i = 1,\; \lambda_i \ge 0,\; i = 0,1,\dots,k \right\}
$$

---
## GJK 迭代

单纯的 GJK 算法不能满足物理引擎的响应需求。我们需要从 GJK 算法中获取一些必要信息以供下一步：**EPA 算法** 所使用。详情参考 [[Chapter-2-EPA算法：静态碰撞信息生成]]

在最后我们将给出 GJK 函数的返回值定义。
### 符号约定

- $A, B$ ：两个待检测的凸体
- $\mathbf{x}^*(\operatorname{shape}, \mathbf{d})$ ：支撑点函数，详细定义见 [[Chapter-0-支撑函数与支撑点]]
- $\mathbf{x}^*_{m}(A, B, \mathbf{d})$ ：**闵可夫斯基差** $A \ominus B$ 在方向 $\mathbf{d}$ 上的支撑点函数：$$
	\mathbf{x}^*_{m}(A, B, \mathbf{d}) = \mathbf{x}^*(A, \mathbf{d}) - \mathbf{x}^*(B, \mathbf{-d})
	$$
- $\sigma$：当前构造的单纯形，储存为顶点的集合（或表示为一个由顶底构成的矩阵）
- $S_A, S_B$ ：搜索过程中凸体 A、B 的所有的支撑点的集合
### 流程
#### 1. 初始化
1. **选定初始方向**。原则上允许任何非零向量。习惯上使用两个物体的中心连线方向：

	 $$
	 \mathbf{d} = \text{center}(B) - \text{center}(A)

	 $$

   若中心重合，任意指定，例如 $\mathbf{d} = (0, 0, \cdots, 1)$
2. **获取第一个支撑点**：$\mathbf{p}_0 = \mathbf{x}^*_{m}(A, B, \mathbf{d})$
3. **初始化单纯形**：$\sigma = \{ \mathbf{p}_0 \}$
4. **更新搜索方向**：$\mathbf{d} = -\mathbf{p}_0$
---
#### 2. 主迭代循环

重复迭代下述步骤，直到判定碰撞或确定未碰撞：
1. **获取新支撑点**：$\mathbf{p} = \mathbf{x}^*_{m}(A, B, \mathbf{d})$，并将 $\mathbf{x}^*(A, \mathbf{d})$ 和 $\mathbf{x}^*(B, \mathbf{-d})$ 分别存入 $S_A$ 和 $S_B$。
2. **检查是否越过原点**：若 $\mathbf{p \cdot d} \lt 0$，则说明在当前搜索方向上，闵可夫斯基差的最远点都没有约过原点，原点不可能在差集的内部，则判定 **未发生碰撞**，算法终止。
3. **将新支撑点加入单纯形**：$\sigma = \sigma \bigcup \set{ \space \mathbf{p} \space }$
4. **检查是否包含原点**：执行子函数 $C_{o}(\sigma, \mathbf{d})$，此函数包含 下述的行为：
	- 判断当前的单纯形是否 **包含原点**：[[#3. 子函数 $C_o( sigma, mathbf{d})$ 原点存在性判定]]
	- 如果包含原点，则判断为 **发生碰撞**，子函数结束并返回
	- 如果不包含，则从单纯形中 **剔除** 不必要的顶点，保留最接近原点 $O$ 的部分：[[#4. 单纯形剔除]]
	- **更新搜索方向** $\mathbf{d}$，新的搜索方向指向原点相对于当前单纯形的 **最近的方向**
---
#### 3. 子函数 $C_o(\sigma, \mathbf{d})$ 原点存在性判定
定义 $n$ 维空间单纯形的顶点为矩阵$\mathbf{V}_{n, k+1}$（下简记为 $\mathbf{V}$）： 
$$
\mathbf{V} = \begin{bmatrix} \mathbf{v}_0 & \mathbf{v}_1 \cdots & \mathbf{v}_{k}  \end{bmatrix}
$$
其中$1 \leq k \leq n + 1$，元素 $\mathbf{v}_i$ 为 $n$ 维空间的点坐标，其定义为：
$$
\mathbf{v}_i = 
\begin{pmatrix} 
v_1 \\ v_2 \\ \vdots \\ v_n
\end{pmatrix}
$$

**定理** $\mathbf{V}_{n, k+1}$ 所确定的单纯形凸包结构是唯一的。

定义重心坐标仿射向量 $\mathbf{\Lambda}$：
$$
\mathbf{\Lambda} = 
\begin{bmatrix} 
\lambda_0 & \lambda_1  & \cdots & \lambda_k
\end{bmatrix}^T
$$

则有 **命题**：
原点 $\mathbf{O} \in \sigma \iff \exists \mathbf{\Lambda}$ 使得矩阵 $\mathbf{V}$ 和向量 $\mathbf{\Lambda}$ 满足下列线性方程组与不等式约束：
$$
\begin{cases}
\mathbf{V \Lambda} = \mathbf{0} \\[10 pt]
\sum_{i = 0}^{k}{\lambda_i} = 1 \\[10pt]
\forall i: \lambda_i \geq 0 \\[10pt]
\end{cases}
$$
一般而言，我们不需要显式地找出这个 $\mathbf{\Lambda}$，只需要判断是否存在满足条件的解即可。

---
#### 4. 单纯形剔除

**最近点问题** 判断原点是否存在凸集 $\sigma$ 内部，则有：
$$
\mathbf{0} \in \sigma \iff \min_{\mathbf{p} \in \sigma} \| p \| = 0
$$
**最近点的数学特征** 设 $\mathbf{p}^*$ 是单纯形上距离原点 $O$ 最近的点，根据 **投影定理**，$\mathbf{p}^*$ 具有如下性质：
$$
\mathbf{p}^* \in \sigma, \mathbf{p}^* \cdot \left( \mathbf{p} - \mathbf{p}^* \right) \geq 0
$$
等价于：$-\mathbf{p}^*$ 是单纯形 $\sigma$ 在 $\mathbf{p}^*$ 处的 **支撑法向量**。

---

子单纯形枚举原理：**单纯形上的最近点一定落在某一个更低维度的子单纯形上**。
**具体描述如下**：
$$
\exists S \subseteq \set{ 0, 1, \cdots, k} \to
\begin{cases} 
\mathbf{p}^* = \sum_{i \in S}{\lambda_i \mathbf{v}_i} \\[10 pt]
\sum_{i \in S}{\lambda_i} = 1 \\[10 pt]
\forall i \in S: \lambda_i \gt 0 \\[10 pt]
\mathbf{p}^* \perp \operatorname{span}{\{\mathbf{v}_i - \mathbf{v}_j: i, j \in S\}} 
\quad \text{(最近点垂直于子单纯形张成的仿射子空间)} \\[10 pt]
\end{cases}
$$
其中符号 $\operatorname{span}$ 表示通过一组向量的 **张成**，即由这组向量的所有有限的线性组合构成的集合。

---
对于所有的候选子集 $S$，我们需要找到在该子单纯形上距离原点最近的点，并检查该点是否确实在子单纯形内部。

**建立线性方程组**

设 $S = \set{i_0, i_1, \cdots , i_m}$，其中 $m \leq k$。需要求解三个线性方程组：
$$
\begin{cases} 
\mathbf{p} = \sum_{j = 0}^{m}{\lambda_j \mathbf{v}_{i_j}} \\[10 pt]
\sum_{j = 0}^{m}{\lambda_j} = 1 \\[10 pt]
\mathbf{p} \cdot \left( \mathbf{v}_{i_j} - \mathbf{v}_{i_0}\right) = 0, \forall j \in \mathbb{N}, 1 \leq j \leq m
 \\[10 pt]
\end{cases}
$$
转化为 **Gram 矩阵**：
$$
\begin{bmatrix} 
\mathbf{v}_{i_0} \cdot \mathbf{v}_{i_0} &
\mathbf{v}_{i_0} \cdot \mathbf{v}_{i_1} &
\cdots                                  & 
\mathbf{v}_{i_0} \cdot \mathbf{v}_{i_m} &  
1         \\[12 pt]
\mathbf{v}_{i_1} \cdot \mathbf{v}_{i_0} & 
\mathbf{v}_{i_1} \cdot \mathbf{v}_{i_1} & 
\cdots                                  & 
\mathbf{v}_{i_1} \cdot \mathbf{v}_{i_m} &  
1         \\[6  pt]
\vdots & \vdots & \ddots & \vdots & \vdots                    
          \\[6  pt]
\mathbf{v}_{i_m} \cdot \mathbf{v}_{i_0} & 
\mathbf{v}_{i_m} \cdot \mathbf{v}_{i_1} & 
\cdots                                  & 
\mathbf{v}_{i_m} \cdot \mathbf{v}_{i_m} &   
1         \\[12 pt]
1 & 1 & \cdots & 1 & 0                                        
          \\[12 pt]
\end{bmatrix}
\begin{bmatrix}
\lambda_{i_0} \\[12 pt]
\lambda_{i_1} \\[6  pt]
\vdots        \\[6  pt]
\lambda_{i_m} \\[12 pt]
\mu
\end{bmatrix}
=
\begin{bmatrix}
0      \\[12 pt]
0      \\[6  pt] 
\vdots \\[6  pt] 
0      \\[12 pt]
1      \\[12 pt]
\end{bmatrix}
$$
其中 $\mu$ 是 **拉格朗日乘子**

---
**可行性检查与记录**

求解 **Gram 矩阵** 后，得到候选重心坐标 $\mathbf{\Lambda}^{(S)}$。只有当满足条件 $\forall i \in S: \lambda_i \gt 0$ 时，这个候选点才是有效的。如果某个 $\lambda_i = 0$，则说明最近点实际上落在更低维度的子单纯形上，这个子集会被另一个更小的子集捕获。

---
**选择最优解**

在所有的有效子单纯形中，选择使得距离最小的那个：
$$
S^* = \arg {\min_{S \subseteq \set{0, 1, \cdots, k}}{\| \sum_{i\in S}{\lambda_i^{(S)}\mathbf{v}_i}\|}}
$$

---

最后，利用最优的有效单纯形 $S^*$ 及其对应的重心坐标 $\mathbf{\Lambda}^{(S)}$ 计算 $\mathbf{p}^*$，执行下述判定：
$$
\mathbf{0} \in \sigma \iff \| \mathbf{p}^* \| = 0
$$
工程上，$0$ 通常被容差 $\epsilon \gt 0$ 代替：
$$
\mathbf{0} \in \sigma \iff \| \mathbf{p}^* \| \lt \epsilon
$$
## 数据处理

最终，GJK 需要向 EPA 阶段提供四项信息：
$$
\operatorname{GJK} = \set{\text{collision}, \mathbf{V}_{\sigma}, \set{S_A, S_B}, \set{d_0, \mathbf{a}_0, \mathbf{b}_0}}
$$
- $\text{collision}$：是否发生碰撞的布尔值
- $\mathbf{V}_{\sigma}$：主迭代中获得的单纯形 $\sigma$ 的顶点坐标所构成的矩阵。其行数为空间维数，列数为单纯形的维数+1
- $\set{S_A, S_B}$ ：（前文已提及）搜索过程中凸体 A、B 的所有的支撑点的集合
- $\set{d_0, \mathbf{a}_0, \mathbf{b}_0}$ 通常情况下，这些数据仅在 **未碰撞** 时有意义。根据 [[#4. 单纯形剔除]] 对最终的 $\mathbf{V}_{\sigma}$ 进行处理，获得初始间隙、物体最近点。具体规则如下：
	- 计算最优的有效单纯形子集 $S^*_{\sigma}$，计算对应的 $\mathbf{p}^*$，令 $d_0 = \|\mathbf{p}^*\|$。
	- 依照下述公式计算 $\mathbf{a}_0, \mathbf{b}_0$
	$$
	\mathbf{a}_0 = \sum_{i \in S^*_{\sigma}}{\lambda_i^{(S^*_{\sigma})} \mathbf{a}_i} \quad \mathbf{b}_0 = \sum_{i \in S^*_{\sigma}}{\lambda_i^{(S^*_{\sigma})} \mathbf{b}_i}
	$$