//
// 由 GitHub Copilot 于 2026/4/25 创建。
//

#ifndef EGRET_PHYSICS_WORLD_SCENE_MANAGER_H
#define EGRET_PHYSICS_WORLD_SCENE_MANAGER_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Eigen/Dense"

#include "field_base.h"
#include "constraints_base.h"
#include "particle.h"
#include "rigid_body.h"
#include "shape_base.h"
#include "solver.h"
#include "transform.h"

namespace egret
{

    /**
     * @brief 世界中的实体记录，保存所有权与可渲染/可求解数据。
     */
    struct BodyRecord
    {
        /** 实体 ID。 */
        std::uint64_t id{0};

        /** 实体名称。 */
        std::string name;

        /** 物理实体的所有权。 */
        std::shared_ptr<PhysicalEntity> entity;

        // /** 碰撞形状的所有权。 */
        // std::unique_ptr<ShapeBase> shape;

        /** 是否参与碰撞。 */
        bool enableCollision{true};

        /** 是否参与积分。 */
        bool enableIntegration{true};

        /** 恢复系数。 */
        // 更新说明：恢复系数在 PhysicalEntity 中被定义
        // double restitution{0.2};
    };

    /**
     * @brief 世界中的场记录，保存场实例与名称。
     */
    struct FieldRecord
    {
        /** 场 ID。 */
        std::uint64_t id{0};

        /** 场名称。 */
        std::string name;

        /** 场的所有权。 */
        std::shared_ptr<FieldBase> field;
    };

    /**
     * @brief 世界中的约束记录，保存约束实例与名称。
     */
    struct ConstraintRecord
    {
        /** 约束 ID。 */
        std::uint64_t id{0};

        /** 约束名称。 */
        std::string name;

        /** 约束的所有权。 */
        std::shared_ptr<ConstraintsBase> constraint;

        /** 是否启用。 */
        bool enabled{true};
    };

    struct SceneRecord
    {
        std::vector<BodyRecord> bodies;
        std::vector<FieldRecord> fields;
        std::vector<ConstraintRecord> constraints;
    };

    class SolverBase;

    /**
     * @brief 用于视图层渲染的轻量化数据快照。
     *
     * 该结构不包含任何拥有权，只提供渲染所需的几何与样式信息，
     * 便于 ViewModel 层转换为 QML 可消费的数据模型。
     */
    // ReSharper disable once CppMismatchedClassTags
    struct SceneRenderItem
    {
        /** 实体 ID。 */
        std::uint64_t id{0};

        /** 渲染类型，例如 sphere、box、particle。 */
        std::uint32_t kind;

        /** 左上角 X 坐标。 */
        double x{0.0};

        /** 左上角 Y 坐标。 */
        double y{0.0};

        /** 渲染宽度。 */
        double width{0.0};

        /** 渲染高度。 */
        double height{0.0};

        /** 世界中心 X。 */
        double centerX{0.0};

        /** 世界中心 Y。 */
        double centerY{0.0};

        /** 世界中心 Z（z-up）。 */
        double centerZ{0.0};

        /** 速度 X。 */
        double speedX{0.0};

        /** 速度 Y。 */
        double speedY{0.0};

        /** 速度 Z。 */
        double speedZ{0.0};

        /** 世界尺寸 X。 */
        double sizeX{0.0};

        /** 世界尺寸 Y。 */
        double sizeY{0.0};

        /** 世界尺寸 Z。 */
        double sizeZ{0.0};

        /** 十六进制颜色字符串，供 QML 直接使用。 */
        std::string color;

        /** 标签文本，用于调试或悬浮提示。 */
        std::string label;

        /** 旋转矩阵元素 (row-major)：m11, m12, m13, m21, m22, m23, m31, m32, m33。 */
        double rotation[9]{1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    };

    /**
     * @brief 模型层场景管理器，负责世界真相数据、时间、ID 和求解调度。
     *
     * 职责说明：
     * 1. 管理实体与场的创建、删除、遍历和查询。
     * 2. 维护仿真时间与步数。
     * 3. 作为求解器的场景快照适配器，向求解器提供只读/可写句柄。
     * 4. 为 ViewModel 层提供可渲染的轻量化数据快照。
     */
    class WorldSceneManager final : public SolverSceneSnapshotBase
    {
    public:
        /**
         * @brief 构造世界场景管理器。
         * @param solver 负责推进世界状态的求解器实例。
         */
        explicit WorldSceneManager(std::unique_ptr<SolverBase> solver);

        /** 默认虚析构。 */
        ~WorldSceneManager() override = default;

        /**
         * @brief 创建任意形状实体（通用入口）。
         * @param name 实体名称。
         * @param position 初始位置。
         * @param speed 初始速度。
         * @param shape 形状所有权（可由内置类型或插件提供）。
         * @param mass 质量，0 表示静态体。
         * @return 新实体的 ID，创建失败返回 0。
         */
        std::uint64_t spawnBody(const std::string& name,
                                const Eigen::Vector3d& position,
                                const Eigen::Vector3d& speed,
                                std::unique_ptr<ShapeBase> shape,
                                double mass);

        /**
         * @brief 注册已存在的物理实体作为世界中的 Body。
         * @param name 实体名称。
         * @param entity 物理实体共享所有权。
         * @param shape 形状所有权。
         * @return 新实体的 ID，失败返回 0。
         */
        std::uint64_t registerBody(const std::string& name,
                                   const std::shared_ptr<PhysicalEntity>& entity,
                                   std::unique_ptr<ShapeBase> shape);



        /**
         * @brief 通过形状加载信息创建实体。
         * @param name 实体名称。
         * @param position 初始位置。
         * @param speed 初始速度。
         * @param shapeInfo 形状加载信息。
         * @param mass 质量，0 表示静态体。
         * @return 新实体的 ID，创建失败返回 0。
         */
        std::uint64_t spawnBodyFromLoadInfo(const std::string& name,
                                            const Eigen::Vector3d& position,
                                            const Eigen::Vector3d& speed,
                                            const ShapeLoadInfo& shapeInfo,
                                            double mass);

        /**
         * @brief 创建一个球体实体。
         * @param name 实体名称。
         * @param position 初始位置。
         * @param speed 初始速度。
         * @param radius 球体半径。
         * @param mass 质量，0 表示静态体。
         * @return 新实体的 ID。
         */
        std::uint64_t spawnSphere(const std::string& name,
                                  const Eigen::Vector3d& position,
                                  const Eigen::Vector3d& speed,
                                  double radius,
                                  double mass);

        /**
         * @brief 创建一个盒体实体。
         * @param name 实体名称。
         * @param position 初始位置。
         * @param speed 初始速度。
         * @param size 盒体尺寸。
         * @param mass 质量，0 表示静态体。
         * @return 新实体的 ID。
         */
        std::uint64_t spawnBox(const std::string& name,
                               const Eigen::Vector3d& position,
                               const Eigen::Vector3d& speed,
                               const Eigen::Vector3d& size,
                               double mass);

        /**
         * @brief 创建一个圆柱体实体。
         * @param name 实体名称。
         * @param position 初始位置。
         * @param speed 初始速度。
         * @param radius 圆柱体半径。
         * @param height 圆柱体高度。
         * @param mass 质量，0 表示静态体。
         * @return 新实体的 ID。
         */
        std::uint64_t spawnCylinder(const std::string& name,
                                    const Eigen::Vector3d& position,
                                    const Eigen::Vector3d& speed,
                                    double radius,
                                    double height,
                                    double mass);

        /**
         * @brief 获取实体对应形状的加载信息（由 Shape 多态提供）。
         * @param id 实体 ID。
         * @return 找到则返回加载信息，否则返回空。
         */
        [[nodiscard]] std::optional<ShapeLoadInfo> getBodyShapeLoadInfo(std::uint64_t id) const;

        /**
         * @brief 读取实体位置。
         * @param id 实体 ID。
         * @return 找到则返回位置，否则返回空。
         */
        [[nodiscard]] std::optional<Eigen::Vector3d> getBodyPosition(std::uint64_t id) const;

        /**
         * @brief 设置实体位置。
         * @param id 实体 ID。
         * @param position 新位置。
         * @return 设置是否成功。
         */
        bool setBodyPosition(std::uint64_t id, const Eigen::Vector3d& position);

        /**
         * @brief 读取实体旋转
         * @param id 实体 ID。
         * @return 找到则返回旋转，否则返回空。
         */
        [[nodiscard]] std::optional<Eigen::Quaterniond> getBodyRotationQuat(std::uint64_t id) const;

        /**
         * @brief 读取实体旋转矩阵
         * @param id 实体 ID。
         * @return 找到则返回旋转，否则返回空。
         */
        [[nodiscard]] std::optional<Eigen::Matrix3d> getBodyRotationMat(std::uint64_t id) const;

        /**
         * @brief 设置实体旋转
         * @param id 实体 ID。
         * @param rotationQuat 新的旋转四元数
         * @return 找到则返回旋转，否则返回空。
         */
        bool setBodyRotation(std::uint64_t id, const Eigen::Quaterniond& rotationQuat);

        /**
         * @brief 设置实体旋转(矩阵版本)
         * @param id 实体 ID。
         * @param rotationMat 新的旋转矩阵
         * @return 找到则返回旋转，否则返回空。
         */
        bool setBodyRotation(std::uint64_t id, const Eigen::Matrix3d& rotationMat);

        /**
         * @brief 添加一个重力场。
         * @param gravity 重力向量。
         * @param referencePoint 势能参考点。
         * @param name 场名称。
         * @return 新场的 ID。
         */
        std::uint64_t addGravityField(const Eigen::Vector3d& gravity,
                                      const Eigen::Vector3d& referencePoint,
                                      const std::string& name = "Gravity");

        /**
         * @brief 注册已存在的场实例。
         * @param name 场名称。
         * @param field 场共享所有权。
         * @return 新场的 ID，失败返回 0。
         */
        std::uint64_t registerField(const std::string& name,
                                    const std::shared_ptr<FieldBase>& field);

        /**
         * @brief 将同一对象同时注册为实体与场。
         * @param bodyName 实体名称。
         * @param fieldName 场名称。
         * @param entity 物理实体共享所有权。
         * @param field 场共享所有权（应与 entity 共享同一控制块）。
         * @param shape 形状所有权。
         * @return 共享 ID，失败返回 0。
         */
        std::uint64_t registerBodyField(const std::string& bodyName,
                                        const std::string& fieldName,
                                        const std::shared_ptr<PhysicalEntity>& entity,
                                        const std::shared_ptr<FieldBase>& field,
                                        std::unique_ptr<ShapeBase> shape);



        void registerScene(const SceneRecord& record);

        /**
         * @brief 按 ID 删除实体。
         * @param id 实体 ID。
         * @return 删除是否成功。
         */
        bool removeBody(std::uint64_t id);

        /**
         * @brief 按 ID 删除场。
         * @param id 场 ID。
         * @return 删除是否成功。
         */
        bool removeField(std::uint64_t id);

        /**
         * @brief 同时按 ID 删除实体与场记录。
         * @param id 实体/场共享的 ID。
         * @return 是否删除了任一记录。
         */
        bool removeBodyField(std::uint64_t id);

        /**
         * @brief 清空整个场景，包括实体、场和时间状态。
         */
        void clear();

        /**
         * @brief 执行一次固定步长推进。
         * @param dt 固定时间步长，单位为秒。
         * @return 求解器返回的统计结果。
         */
        SolverStepResult tick(double dt);

        /**
         * @brief 生成供 ViewModel 渲染的轻量化数据快照。
         * @return 渲染条目列表。
         */
        [[nodiscard]] std::vector<SceneRenderItem> buildRenderItems() const;

        /**
         * @brief 查询当前实体数量。
         * @return 实体数量。
         */
        [[nodiscard]] std::size_t getBodyCount() const;

        /**
         * @brief 查询当前场数量。
         * @return 场数量。
         */
        [[nodiscard]] std::size_t getFieldCount() const;

        /**
         * @brief 查询当前步数。
         * @return 已完成步数。
         */
        [[nodiscard]] std::size_t getStepCount() const;

        /**
         * @brief 查询当前仿真时间。
         * @return 仿真时间，单位为秒。
         */
        [[nodiscard]] double getSimulationTime() const override;

        /**
         * @brief 推进仿真时钟。
         * @param dt 固定步长，单位为秒。
         */
        void advanceSimulationTime(double dt) override;

        /**
         * @brief 返回实体句柄的可变连续视图，供求解器读写。
         * @return 实体句柄 span。
         */
        [[nodiscard]] std::span<SolverBodyHandle> getBodies() override;

        /**
         * @brief 返回实体句柄的只读连续视图，供求解器只读访问。
         * @return 只读实体句柄 span。
         */
        [[nodiscard]] std::span<const SolverBodyHandle> getBodies() const override;

        /**
         * @brief 返回场指针的可变连续视图。
         * @return 场指针 span。
         */
        [[nodiscard]] std::span<FieldBase*> getFields() override;

        /**
         * @brief 返回场指针的只读连续视图。
         * @return 只读场指针 span。
         */
        [[nodiscard]] std::span<FieldBase*const> getFields() const override;

        /**
         * @brief 为 ViewModel 层提供当前活动实体的名称列表。
         * @return 实体名称列表。
         */
        [[nodiscard]] std::vector<std::string> getBodyNames() const;

        /**
         * @brief 注册约束器到场景。
         * @param name 约束名称。
         * @param constraint 约束共享所有权。
         * @return 新约束的 ID，失败返回 0。
         */
        std::uint64_t registerConstraint(const std::string& name,
                                         const std::shared_ptr<ConstraintsBase>& constraint);

        /**
         * @brief 创建并注册折线约束器。
         * @param name 约束名称。
         * @param length 最大折线长度。
         * @param entityStartId 起始实体 ID。
         * @param entityEndId 终止实体 ID。
         * @param turningPositions 路径点列表（可选）。
         * @return 新约束的 ID，失败返回 0。
         */
        std::uint64_t createConnectingLine(const std::string& name,
                                           double length,
                                           std::uint64_t entityStartId,
                                           std::uint64_t entityEndId,
                                           const std::initializer_list<Eigen::Vector3d>& turningPositions = {});

        /**
         * @brief 创建并注册折线约束器。
         * @param name 约束名称。
         * @param length 最大折线长度。
         * @param anchorPos 锚点位置
         * @param entityId 实体 ID。
         * @return 新约束的 ID，失败返回 0。
         */
        std::uint64_t createSimplePendulum(const std::string& name,
                                           double length, const Eigen::Vector3d& anchorPos, std::uint64_t entityId);


        /**
         * @brief 获取约束器。
         * @param id 约束 ID。
         * @return 找到则返回约束指针，否则返回 nullptr。
         */
        [[nodiscard]] ConstraintsBase* getConstraint(std::uint64_t id);

        /**
         * @brief 获取所有约束。
         * @return 约束指针列表。
         */
        [[nodiscard]] std::vector<ConstraintsBase*> getAllConstraints() const;

        /**
         * @brief 移除约束器。
         * @param id 约束 ID。
         * @return 移除是否成功。
         */
        bool removeConstraint(std::uint64_t id);

        /**
         * @brief 启用/禁用约束。
         * @param id 约束 ID。
         * @param enabled 是否启用。
         * @return 设置是否成功。
         */
        bool setConstraintEnabled(std::uint64_t id, bool enabled);

        /**
         * @brief 获取约束误差（用于调试）。
         * @param id 约束 ID。
         * @return 约束误差值。
         */
        [[nodiscard]] double getConstraintError(std::uint64_t id) const;

        /**
         * @brief 添加路径点。
         * @param constraintId 约束 ID。
         * @param index 插入位置索引。
         * @param pos 路径点位置。
         * @return 操作是否成功。
         */
        bool addTurningPoint(std::uint64_t constraintId, size_t index, const Eigen::Vector3d& pos);

        /**
         * @brief 移除路径点。
         * @param constraintId 约束 ID。
         * @param index 路径点索引。
         * @return 操作是否成功。
         */
        bool removeTurningPoint(std::uint64_t constraintId, size_t index);

        /**
         * @brief 修改路径点。
         * @param constraintId 约束 ID。
         * @param index 路径点索引。
         * @param newPos 新位置。
         * @return 操作是否成功。
         */
        bool changeTurningPoint(std::uint64_t constraintId, size_t index, const Eigen::Vector3d& newPos);

        /**
         * @brief 返回活动约束的可变连续视图。
         * @return 约束指针的 span。
         */
        [[nodiscard]] std::span<ConstraintsBase*> getConstraints() override;

        /**
         * @brief 返回活动约束的只读连续视图。
         * @return 约束指针的 const span。
         */
        [[nodiscard]] std::span<const ConstraintsBase*> getConstraints() const override;




    private:
        /**
         * @brief 重建求解器缓存，确保句柄指向当前记录地址。
         */
        void rebuildSolverCaches() const;

        /**
         * @brief 重建实体句柄缓存。
         */
        void rebuildSolverBodyCache() const;

        /**
         * @brief 重建场指针缓存。
         */
        void rebuildSolverFieldCache() const;

        /**
         * @brief 重建约束指针缓存。
         */
        void rebuildSolverConstraintCache() const;

        /**
         * @brief 生成新的世界唯一 ID。
         * @return 新 ID。
         */
        std::uint64_t nextId();

        /**
         * @brief 查找实体记录。
         * @param id 实体 ID。
         * @return 找到则返回记录指针，否则返回空指针。
         */
        BodyRecord* findBody(std::uint64_t id);

        /**
         * @brief 查找场记录。
         * @param id 场 ID。
         * @return 找到则返回记录指针，否则返回空指针。
         */
        FieldRecord* findField(std::uint64_t id);

        /**
         * @brief 查找实体记录的只读版本。
         * @param id 实体 ID。
         * @return 找到则返回记录指针，否则返回空指针。
         */
        const BodyRecord* findBody(std::uint64_t id) const;

        /**
         * @brief 查找场记录的只读版本。
         * @param id 场 ID。
         * @return 找到则返回记录指针，否则返回空指针。
         */
        const FieldRecord* findField(std::uint64_t id) const;

        /**
         * @brief 查找约束记录。
         * @param id 约束 ID。
         * @return 找到则返回记录指针，否则返回空指针。
         */
        ConstraintRecord* findConstraint(std::uint64_t id);

        /**
         * @brief 查找约束记录的只读版本。
         * @param id 约束 ID。
         * @return 找到则返回记录指针，否则返回空指针。
         */
        const ConstraintRecord* findConstraint(std::uint64_t id) const;

        /** 求解器所有权。 */
        std::unique_ptr<SolverBase> m_solver;

        /** 世界实体记录。 */
        std::vector<std::unique_ptr<BodyRecord>> m_bodies;

        /** 世界场记录。 */
        std::vector<std::unique_ptr<FieldRecord>> m_fields;

        /** 给求解器使用的实体句柄缓存。 */
        mutable std::vector<SolverBodyHandle> m_solverBodies;

        /** 持有实体以保证求解期间生命周期稳定。 */
        mutable std::vector<std::shared_ptr<PhysicalEntity>> m_solverBodyOwners;

        /** 给求解器使用的场指针缓存。 */
        mutable std::vector<FieldBase*> m_solverFields;

        /** 持有场以保证求解期间生命周期稳定。 */
        mutable std::vector<std::shared_ptr<FieldBase>> m_solverFieldOwners;

        /** 世界约束记录。 */
        std::vector<std::unique_ptr<ConstraintRecord>> m_constraints;

        /** 给求解器使用的约束指针缓存。 */
        mutable std::vector<const ConstraintsBase*> m_solverConstraints;

        /** 持有约束以保证求解期间生命周期稳定。 */
        mutable std::vector<std::shared_ptr<ConstraintsBase>> m_solverConstraintOwners;

        /** 当前仿真时间。 */
        double m_simulationTime{0.0};

        /** 已完成的步数。 */
        std::size_t m_stepCount{0};

        /** 递增 ID 生成器。 */
        std::uint64_t m_nextId{1};
    };
}

#endif // EGRET_PHYSICS_WORLD_SCENE_MANAGER_H
