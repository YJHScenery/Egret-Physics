/**
 * @file        connecting_rod.h
 * @brief       连接杆约束器头文件
 * @details     定义 Endpoint 变体结构、RodConnection 结构、
 *              RodJointState 枚举和 ConnectingRod 类，
 *              提供刚性杆约束及杆间关节功能。
 *
 *              杆的每一端（Endpoint）可以是：
 *              - 物理实体上的本地锚点（Entity 类型）
 *              - 另一根杆的某一端（Rod 类型，形成虚拟铰接点）
 *
 *              多根杆可以通过端点互连形成链或网络拓扑，
 *              每对连接独立维护角度约束状态。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-15
 * @version     2.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_CONNECTING_ROD_H
#define EGRET_PHYSICS_CONNECTING_ROD_H

#include <cstdint>
#include <vector>

#include "constraints_base.h"
#include "Eigen/Dense"

namespace egret
{
    class PhysicalEntity;
    class ConnectingRod;

    // ========================================================================
    //  枚举
    // ========================================================================

    /**
     * @brief       端点类型枚举。
     * @details     描述 ConnectingRod 某一端连接的目标类型。
     */
    enum class EndpointType : std::uint8_t
    {
        Entity,  ///< 连接到物理实体上的本地锚点
        Rod      ///< 连接到另一根杆的某一端点（虚拟铰接点）
    };

    /**
     * @brief       杆间关节状态枚举。
     * @details     描述两根 ConnectingRod 在共享端点处的约束模式。
     *              - Free：等效于球铰，两杆可绕共享端点任意旋转。
     *              - Coplanar：两杆限制在参考平面内。
     *              - FixedAngle：杆间夹角锁定为参考值。
     */
    enum class RodJointState : std::uint8_t
    {
        Free,        ///< 自由 —— 仅球铰约束
        Coplanar,    ///< 共面 —— 两杆限制在参考平面内
        FixedAngle   ///< 固定角 —— 杆间夹角固定
    };

    // ========================================================================
    //  结构体
    // ========================================================================

    /**
     * @brief       杆端点描述结构体。
     * @details     Endpoint 是一个变体结构：type 字段决定哪些成员有效。
     *              - Entity 类型：entity 和 localAnchor 有效。
     *              - Rod 类型：connectedRod 和 connectedRodEndIndex 有效。
     *              世界位置通过递推解析获得——Rod 类型端点最终必然
     *              回溯到一个 Entity 类型端点来计算世界坐标。
     * @note        循环引用（如 Rod1.EndA ↔ Rod2.EndA 且 Rod1.EndB ↔ Rod2.EndB）
     *              为未定义行为，调用方需保证无环。
     */
    struct Endpoint
    {
        /** 端点类型。 */
        EndpointType type{EndpointType::Entity};

        // ---- Entity 类型字段 ----

        /** 绑定的物理实体（type == Entity 时有效）。 */
        PhysicalEntity* entity{nullptr};

        /** 实体本地空间中的连接点（type == Entity 时有效）。 */
        Eigen::Vector3d localAnchor{Eigen::Vector3d::Zero()};

        // ---- Rod 类型字段 ----

        /** 连接的另一根杆（type == Rod 时有效）。 */
        ConnectingRod* connectedRod{nullptr};

        /** 连接的另一根杆的端点索引：0 = EndA，1 = EndB（type == Rod 时有效）。 */
        int connectedRodEndIndex{0};

        /**
         * @brief 缓存的最近一次世界位置。
         * @details 当 type == Rod 时，为避免双向递归，直接返回此缓存值。
         *          缓存由 connectEndToRod 初始化，并在每帧求解后更新。
         */
        Eigen::Vector3d cachedWorldPos{Eigen::Vector3d::Zero()};

        /** 缓存是否有效。仅当 type == Rod 且已连接时为 true。 */
        bool hasCachedPos{false};

        /**
         * @brief 判断是否为 Entity 类型。
         */
        [[nodiscard]] bool isEntity() const { return type == EndpointType::Entity; }

        /**
         * @brief 判断是否为 Rod 类型。
         */
        [[nodiscard]] bool isRod() const { return type == EndpointType::Rod; }
    };

    /**
     * @brief       杆间连接描述结构体。
     * @details     描述本杆的某一端与另一根杆的某一端之间的连接关系
     *              及该连接上的角度约束状态。
     *              一根杆可同时拥有多条连接（多杆汇聚于同一端点）。
     */
    struct RodConnection
    {
        /** 连接的另一根杆。 */
        ConnectingRod* otherRod{nullptr};

        /** 本杆参与连接的端点索引（0 = EndA，1 = EndB）。 */
        int myEndIndex{0};

        /** 另一根杆参与连接的端点索引（0 = EndA，1 = EndB）。 */
        int otherEndIndex{0};

        /** 关节状态。 */
        RodJointState state{RodJointState::Free};

        /** 共面约束的参考平面法线（世界空间单位向量，state ≥ Coplanar 时有效）。 */
        Eigen::Vector3d refPlaneNormal{Eigen::Vector3d::UnitY()};

        /** 固定角约束的参考夹角余弦值（state == FixedAngle 时有效）。 */
        double refCosAngle{1.0};
    };

    // ========================================================================
    //  ConnectingRod 类
    // ========================================================================

    /**
     * @brief       连接杆约束器类，实现两个端点之间的刚性距离约束及杆间关节。
     * @details     ConnectingRod 继承自 ConstraintsBase，在其两个端点（EndA 和 EndB）
     *              之间维持固定杆长。每个端点可以是物理实体上的本地锚点，
     *              也可以是另一根杆的端点（形成虚拟铰接点）。
     *
     *              与 ConnectingLine 的关键区别：
     *              - 刚性双向：距离不足时也施加约束
     *              - Endpoint 变体：杆端可直接对接另一杆端，无需中间实体
     *              - 转动惯量感知：冲量计算包含旋转惯性
     *              - Baumgarte 稳定化 + 位置投影兜底
     *
     *              杆间角度约束通过 RodConnection 列表管理。
     *              在 applyVelocityConstraint 中遍历所有连接，
     *              仅当 this < otherRod（地址比较）时施加以去重。
     *
     * @invariant   m_length > 0
     * @invariant   m_endA 和 m_endB 的递推解析链无环
     * @remark      端点世界位置通过 resolveEndpointWorldPos 递推解析
     * @see         ConstraintsBase, ConnectingLine, SimplePendulum, Endpoint
     */
    class ConnectingRod : public ConstraintsBase
    {
    public:
        // ==================== 构造与析构 ====================

        /**
         * @brief 构造连接杆，杆长由两 Entity 端点的当前世界距离自动计算。
         * @param endA 端点 A（应为 Entity 类型）。
         * @param endB 端点 B（应为 Entity 类型）。
         * @param id 约束器唯一 ID。
         */
        ConnectingRod(const Endpoint& endA, const Endpoint& endB, std::uint64_t id);

        /**
         * @brief 构造连接杆，显式指定杆长。
         * @param endA 端点 A。
         * @param endB 端点 B。
         * @param length 杆长（m），必须 > 0。
         * @param id 约束器唯一 ID。
         */
        ConnectingRod(const Endpoint& endA, const Endpoint& endB,
                      double length, std::uint64_t id);

        ~ConnectingRod() override = default;

        // ==================== 约束接口 ====================

        /** @brief 返回 ConstraintType::ConnectingRod。 */
        [[nodiscard]] ConstraintType getType() const override;

        /**
         * @brief 应用速度约束。
         * @details 执行：
         *          1. 解析两端点世界位置，计算当前距离与方向。
         *          2. 刚性距离约束 —— 含转动惯量 + Baumgarte 稳定化。
         *          3. 遍历 m_connections，对每对连接施加角度约束
         *             （共面 / 固定角，按 this < other 去重）。
         * @param dt 固定时间步长。
         */
        void applyVelocityConstraint(double dt) override;

        /**
         * @brief 应用位置约束（兜底位置投影）。
         * @param dt 固定时间步长。
         */
        void applyPositionConstraint(double dt) override;

        /**
         * @brief 应用角速度约束（当前求解器不调用，保留空实现）。
         * @param dt 固定时间步长（未使用）。
         */
        void applyAngularVelocityConstraint(double dt) override;

        /** @brief 计算约束误差 = |当前距离 - 杆长|。 */
        [[nodiscard]] double computeConstraintError() const override;

        /** @brief 获取参考长度（杆长）。 */
        [[nodiscard]] double getReferenceLength() const override { return m_length; }

        /** @brief 克隆约束器（连接列表中的外部引用会被清除）。 */
        [[nodiscard]] std::unique_ptr<PhysicsAbstract> clone(std::uint64_t id) const override;

        // ==================== 端点与连接管理 ====================

        /**
         * @brief 设置某一端的 Endpoint。
         * @param endIndex 端点索引：0 = EndA，1 = EndB。
         * @param endpoint 新的端点描述。
         */
        void setEndpoint(int endIndex, const Endpoint& endpoint);

        /** @brief 获取 EndA。 */
        [[nodiscard]] const Endpoint& getEndpointA() const { return m_endA; }

        /** @brief 获取 EndB。 */
        [[nodiscard]] const Endpoint& getEndpointB() const { return m_endB; }

        /**
         * @brief 获取端点最终绑定的物理实体（递推解析 Rod 链）。
         * @param endIndex 端点索引：0 = EndA，1 = EndB。
         * @return 最终实体指针，若解析链断裂返回 nullptr。
         */
        [[nodiscard]] PhysicalEntity* getBody(int endIndex) const;

        /** @brief 获取 EndA 最终绑定的实体（便捷方法）。 */
        [[nodiscard]] PhysicalEntity* getBodyA() const { return getBody(0); }

        /** @brief 获取 EndB 最终绑定的实体（便捷方法）。 */
        [[nodiscard]] PhysicalEntity* getBodyB() const { return getBody(1); }

        /**
         * @brief 将本杆的一端连接到另一根杆的一端。
         * @param myEndIndex 本杆端点索引（0 或 1）。
         * @param otherRod 另一根杆。
         * @param otherEndIndex 另一根杆的端点索引（0 或 1）。
         * @param state 关节状态。若为 Coplanar/FixedAngle，基于当前状态捕获参考值。
         * @note 此方法会修改 m_endA 或 m_endB 的 type 为 Rod。
         */
        void connectEndToRod(int myEndIndex, ConnectingRod* otherRod,
                             int otherEndIndex, RodJointState state);

        /**
         * @brief 断开本杆某一端的连接，将其重置为无效状态。
         * @param endIndex 端点索引（0 或 1）。
         * @note 同时移除所有涉及该端点的 RodConnection 记录。
         */
        void disconnectEnd(int endIndex);

        /** @brief 获取连接数量。 */
        [[nodiscard]] int getConnectionCount() const { return static_cast<int>(m_connections.size()); }

        /** @brief 获取指定索引的连接记录（只读）。 */
        [[nodiscard]] const RodConnection& getConnection(int index) const { return m_connections[index]; }

        /**
         * @brief 修改指定连接的关节状态。
         * @param connIndex 连接索引。
         * @param state 新状态。若为 Coplanar/FixedAngle，基于当前状态重新捕获参考值。
         */
        void setJointState(int connIndex, RodJointState state);

        /** @brief 获取指定连接的关节状态。 */
        [[nodiscard]] RodJointState getJointState(int connIndex) const;

        // ==================== 访问器 ====================

        /** @brief 获取杆长（m）。 */
        [[nodiscard]] double getLength() const { return m_length; }

        /**
         * @brief 设置 Baumgarte 稳定化系数。
         * @param beta 范围 [0, 1]，默认 0.2。
         */
        void setBaumgarteBeta(double beta) { m_baumgarteBeta = std::clamp(beta, 0.0, 1.0); }

        /** @brief 获取 Baumgarte 系数。 */
        [[nodiscard]] double getBaumgarteBeta() const { return m_baumgarteBeta; }

        /**
         * @brief 递推解析一个端点的世界空间位置。
         * @param end 要解析的端点。
         * @param[out] outPos 解析后的世界坐标。
         * @return 解析成功返回 true，失败（链断裂或无 Entity 端点）返回 false。
         */
        static bool resolveEndpointWorldPos(const Endpoint& end, Eigen::Vector3d& outPos);

        /**
         * @brief 递推解析一个端点最终绑定的物理实体。
         * @param end 要解析的端点。
         * @return 最终实体指针，失败返回 nullptr。
         */
        static PhysicalEntity* resolveEndpointEntity(const Endpoint& end);

        /**
         * @brief 获取端点（可写）。
         */
        Endpoint& endpointAt(int index);

    protected:
        /** @brief 默认构造。 */
        ConnectingRod();

        /** @brief 仅指定 ID 的构造。 */
        explicit ConnectingRod(std::uint64_t id);

    private:
        // ==================== 私有辅助 ====================

        /**
         * @brief 计算含转动惯量的有效质量倒数。
         */
        static double computeEffectiveMassInv(const Eigen::Vector3d& n,
                                              const Eigen::Vector3d& rA,
                                              const Eigen::Vector3d& rB,
                                              const PhysicalEntity* bodyA,
                                              const PhysicalEntity* bodyB);

        /**
         * @brief 对单个物体施加冲量（线速度 + 角速度）。
         */
        static void applyImpulseToBody(PhysicalEntity* body,
                                       const Eigen::Vector3d& impulse,
                                       const Eigen::Vector3d& r);

        /**
         * @brief 对单个物体施加角速度修正。
         */
        static void applyAngularCorrection(PhysicalEntity* body,
                                           const Eigen::Vector3d& angularCorrection);

        /**
         * @brief 应用杆间共面约束。
         */
        void applyCoplanarConstraint(const RodConnection& conn);

        /**
         * @brief 应用杆间固定角约束。
         */
        void applyFixedAngleConstraint(const RodConnection& conn);

        /**
         * @brief 基于当前状态捕获连接的参考值。
         */
        void captureConnectionRefValues(RodConnection& conn);



        /**
         * @brief 获取端点（只读）。
         */
        [[nodiscard]] const Endpoint& endpointAt(int index) const;

        /**
         * @brief 移除所有涉及指定端点的连接记录。
         */
        void removeConnectionsForEnd(int endIndex);

        // ==================== 成员变量 ====================

        /** 端点 A。 */
        Endpoint m_endA;

        /** 端点 B。 */
        Endpoint m_endB;

        /** 约束杆长（m）。 */
        double m_length{0.0};

        /** Baumgarte 稳定化系数。 */
        double m_baumgarteBeta{0.2};

        /** 杆间连接列表。 */
        std::vector<RodConnection> m_connections;
    };
} // egret

#endif // EGRET_PHYSICS_CONNECTING_ROD_H
