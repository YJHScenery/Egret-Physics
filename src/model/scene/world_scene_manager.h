//
// 由 GitHub Copilot 于 2026/4/25 创建。
//

#ifndef EGRET_PHYSICS_WORLD_SCENE_MANAGER_H
#define EGRET_PHYSICS_WORLD_SCENE_MANAGER_H

#include <memory>
#include <string>
#include <vector>

#include "Eigen/Dense"

#include "field_base.h"
#include "particle.h"
#include "rigid_body.h"
#include "shape_box.h"
#include "shape_sphere.h"
#include "solver.h"
#include "transform.h"

namespace egret
{
    class ISolver;

    /**
     * @brief 用于视图层渲染的轻量化数据快照。
     *
     * 该结构不包含任何拥有权，只提供渲染所需的几何与样式信息，
     * 便于 ViewModel 层转换为 QML 可消费的数据模型。
     */
    struct SceneRenderItem
    {
        /** 实体 ID。 */
        std::uint64_t id{0};

        /** 渲染类型，例如 sphere、box、particle。 */
        std::string kind;

        /** 左上角 X 坐标。 */
        double x{0.0};

        /** 左上角 Y 坐标。 */
        double y{0.0};

        /** 渲染宽度。 */
        double width{0.0};

        /** 渲染高度。 */
        double height{0.0};

        /** 十六进制颜色字符串，供 QML 直接使用。 */
        std::string color;

        /** 标签文本，用于调试或悬浮提示。 */
        std::string label;
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
    class WorldSceneManager final : public ISolverSceneSnapshot
    {
    public:
        /**
         * @brief 构造世界场景管理器。
         * @param solver 负责推进世界状态的求解器实例。
         */
        explicit WorldSceneManager(std::unique_ptr<ISolver> solver);

        /** 默认虚析构。 */
        ~WorldSceneManager() override = default;

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
        [[nodiscard]] std::span<FieldBase* const> getFields() const override;

        /**
         * @brief 为 ViewModel 层提供当前活动实体的名称列表。
         * @return 实体名称列表。
         */
        [[nodiscard]] std::vector<std::string> getBodyNames() const;

    private:
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
            std::unique_ptr<PhysicalEntity> entity;

            /** 碰撞形状的所有权。 */
            std::unique_ptr<ShapeBase> shape;

            /** 世界变换。 */
            Transform transform;

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
            std::unique_ptr<FieldBase> field;
        };

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

        /** 求解器所有权。 */
        std::unique_ptr<ISolver> m_solver;

        /** 世界实体记录。 */
        std::vector<std::unique_ptr<BodyRecord>> m_bodies;

        /** 世界场记录。 */
        std::vector<std::unique_ptr<FieldRecord>> m_fields;

        /** 给求解器使用的实体句柄缓存。 */
        mutable std::vector<SolverBodyHandle> m_solverBodies;

        /** 给求解器使用的场指针缓存。 */
        mutable std::vector<FieldBase*> m_solverFields;

        /** 当前仿真时间。 */
        double m_simulationTime{0.0};

        /** 已完成的步数。 */
        std::size_t m_stepCount{0};

        /** 递增 ID 生成器。 */
        std::uint64_t m_nextId{1};
    };
}

#endif // EGRET_PHYSICS_WORLD_SCENE_MANAGER_H
