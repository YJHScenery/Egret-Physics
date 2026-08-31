/**
 * @file        test_sleep.cpp
 * @brief       休眠策略单元测试
 * @details     测试 KineticSleepStrategy 的冻结判定、计数重置、
 *              受力比对解冻、基线近零兜底与固定实体忽略逻辑。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-08-31
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Tests
 * @defgroup    SleepStrategyTests
 */

#define BOOST_TEST_MODULE SleepTests
#include "boost_test_dependency.h"

#include <span>
#include <tuple>
#include <vector>

#include "async_logger.h"
#include "constraints_base.h"
#include "field_base.h"
#include "kinetic_sleep_strategy.h"
#include "particle.h"
#include "physical_entity.h"
#include "solver_scene_snapshot.h"
#include "solver_types.h"

using namespace egret;

namespace
{
    /**
     * @brief       测试用最小场景快照。
     * @details     实现 SolverSceneSnapshotBase，仅承载实体句柄，
     *              场与约束均为空，供休眠策略 evaluate() 使用。
     */
    class MockScene final : public SolverSceneSnapshotBase
    {
    public:
        /** 实体句柄缓冲区。 */
        std::vector<SolverBodyHandle> bodies;

        /** 场缓冲区（测试中保持为空）。 */
        std::vector<FieldBase *> fields;

        /** 约束缓冲区（测试中保持为空）。 */
        std::vector<ConstraintsBase *> constraints;

        std::span<SolverBodyHandle> getBodies() override
        {
            return {bodies.data(), bodies.size()};
        }

        std::span<const SolverBodyHandle> getBodies() const override
        {
            return {bodies.data(), bodies.size()};
        }

        std::span<FieldBase *> getFields() override
        {
            return {fields.data(), fields.size()};
        }

        std::span<FieldBase *const> getFields() const override
        {
            return {fields.data(), fields.size()};
        }

        std::span<ConstraintsBase *> getConstraints() override
        {
            return {constraints.data(), constraints.size()};
        }

        std::span<const ConstraintsBase *> getConstraints() const override
        {
            return {constraints.data(), constraints.size()};
        }

        [[nodiscard]] double getSimulationTime() const override
        {
            return 0.0;
        }

        void advanceSimulationTime(const double dt) override
        {
            std::ignore = dt;
        }
    };

    /**
     * @brief 关闭测试环境的日志文件输出，避免在仓库中产生 app.log。
     */
    void disableLogFileOutput()
    {
        AsyncLogger::instance().setOutputToFile(false);
    }

    /**
     * @brief 运行一个渲染帧（一个计算帧 + 渲染帧结算）。
     * @param strategy 休眠策略。
     * @param scene 测试场景快照。
     */
    void runOneRenderFrame(KineticSleepStrategy &strategy, MockScene &scene)
    {
        constexpr double kDt = 1.0 / 120.0;
        strategy.evaluate(scene, kDt);
        strategy.onRenderFrameEnd();
    }
}

BOOST_AUTO_TEST_CASE(test_sleep_freezes_low_speed_entity)
{
    disableLogFileOutput();

    Particle particle(Eigen::Vector3d::Zero(), Eigen::Vector3d::Zero(), 1.0, 1);
    MockScene scene;
    scene.bodies.push_back(SolverBodyHandle{.entity = &particle, .inverseMass = 1.0});

    SleepConfig config{};
    config.enabled = true;
    config.metricType = SleepMetricType::SpeedSquared;
    config.speedSquaredThreshold = 1e-4;
    config.frozenFrameCount = 3;

    KineticSleepStrategy strategy(config);

    for (int frame = 0; frame < 3; ++frame) {
        runOneRenderFrame(strategy, scene);
    }

    BOOST_CHECK(strategy.isAsleep(1));
    BOOST_CHECK(particle.isAsleep());
    BOOST_CHECK_SMALL(particle.getVelocity().norm(), 1e-12);
}

BOOST_AUTO_TEST_CASE(test_sleep_ignores_moving_entity)
{
    disableLogFileOutput();

    Particle particle(Eigen::Vector3d::Zero(), Eigen::Vector3d(1.0, 0.0, 0.0), 1.0, 2);
    MockScene scene;
    scene.bodies.push_back(SolverBodyHandle{.entity = &particle, .inverseMass = 1.0});

    SleepConfig config{};
    config.enabled = true;
    config.frozenFrameCount = 3;

    KineticSleepStrategy strategy(config);

    for (int frame = 0; frame < 10; ++frame) {
        runOneRenderFrame(strategy, scene);
    }

    BOOST_CHECK(!strategy.isAsleep(2));
    BOOST_CHECK(!particle.isAsleep());
}

BOOST_AUTO_TEST_CASE(test_sleep_counter_resets_on_motion)
{
    disableLogFileOutput();

    Particle particle(Eigen::Vector3d::Zero(), Eigen::Vector3d::Zero(), 1.0, 3);
    MockScene scene;
    scene.bodies.push_back(SolverBodyHandle{.entity = &particle, .inverseMass = 1.0});

    SleepConfig config{};
    config.enabled = true;
    config.frozenFrameCount = 3;

    KineticSleepStrategy strategy(config);

    // 帧 1、2：静止通过
    runOneRenderFrame(strategy, scene);
    runOneRenderFrame(strategy, scene);

    // 帧 3：运动 → 连续计数清零
    particle.setSpeed(Eigen::Vector3d(1.0, 0.0, 0.0));
    runOneRenderFrame(strategy, scene);
    BOOST_CHECK(!strategy.isAsleep(3));

    // 帧 4、5：恢复静止 → 重新计数
    particle.setSpeed(Eigen::Vector3d::Zero());
    runOneRenderFrame(strategy, scene);
    runOneRenderFrame(strategy, scene);
    BOOST_CHECK(!strategy.isAsleep(3));

    // 帧 6：连续第 3 帧静止 → 冻结
    runOneRenderFrame(strategy, scene);
    BOOST_CHECK(strategy.isAsleep(3));
}

BOOST_AUTO_TEST_CASE(test_sleep_wakes_on_force_change)
{
    disableLogFileOutput();

    Particle particle(Eigen::Vector3d::Zero(), Eigen::Vector3d::Zero(), 1.0, 4);
    // 冻结前施加恒定重力类外力，作为受力基线
    particle.addForce(Force{.force = Eigen::Vector3d(0.0, -9.8, 0.0), .id = 100});

    MockScene scene;
    scene.bodies.push_back(SolverBodyHandle{.entity = &particle, .inverseMass = 1.0});

    SleepConfig config{};
    config.enabled = true;
    config.frozenFrameCount = 1;
    config.wakeUpForceRatio = 0.01;
    config.wakeUpAbsoluteForceThreshold = 1e-4;

    KineticSleepStrategy strategy(config);

    // 一个渲染帧后冻结，基线合力 = (0, -9.8, 0)
    runOneRenderFrame(strategy, scene);
    BOOST_CHECK(strategy.isAsleep(4));

    // 受力不变 → 保持冻结
    strategy.evaluate(scene, 1.0 / 120.0);
    BOOST_CHECK(strategy.isAsleep(4));

    // 叠加 (0, -1, 0)：向量差模长 1 > 1% × 9.8 = 0.098 → 解冻
    particle.addForce(Force{.force = Eigen::Vector3d(0.0, -1.0, 0.0), .id = 101});
    strategy.evaluate(scene, 1.0 / 120.0);

    BOOST_CHECK(!strategy.isAsleep(4));
    BOOST_CHECK(!particle.isAsleep());
}

BOOST_AUTO_TEST_CASE(test_sleep_wake_absolute_threshold_fallback)
{
    disableLogFileOutput();

    Particle particle(Eigen::Vector3d::Zero(), Eigen::Vector3d::Zero(), 1.0, 5);

    MockScene scene;
    scene.bodies.push_back(SolverBodyHandle{.entity = &particle, .inverseMass = 1.0});

    SleepConfig config{};
    config.enabled = true;
    config.frozenFrameCount = 1;
    config.wakeUpForceRatio = 0.01;
    config.wakeUpAbsoluteForceThreshold = 1e-4;

    KineticSleepStrategy strategy(config);

    // 无外力冻结，基线合力为 0
    runOneRenderFrame(strategy, scene);
    BOOST_CHECK(strategy.isAsleep(5));

    // 微小扰动 1e-5 N < 绝对阈值 1e-4 N → 保持冻结
    particle.addForce(Force{.force = Eigen::Vector3d(1e-5, 0.0, 0.0), .id = 200});
    strategy.evaluate(scene, 1.0 / 120.0);
    BOOST_CHECK(strategy.isAsleep(5));

    // 明显扰动 1e-2 N > 绝对阈值 → 解冻
    particle.addForce(Force{.force = Eigen::Vector3d(1e-2, 0.0, 0.0), .id = 201});
    strategy.evaluate(scene, 1.0 / 120.0);
    BOOST_CHECK(!strategy.isAsleep(5));
}

BOOST_AUTO_TEST_CASE(test_sleep_ignores_fixed_entity)
{
    disableLogFileOutput();

    // 质量 0 表示固定实体，不参与休眠判定
    Particle fixedParticle(Eigen::Vector3d::Zero(), Eigen::Vector3d::Zero(), 0.0, 6);

    MockScene scene;
    scene.bodies.push_back(SolverBodyHandle{.entity = &fixedParticle, .inverseMass = 0.0});

    SleepConfig config{};
    config.enabled = true;
    config.frozenFrameCount = 1;

    KineticSleepStrategy strategy(config);

    for (int frame = 0; frame < 3; ++frame) {
        runOneRenderFrame(strategy, scene);
    }

    BOOST_CHECK(!strategy.isAsleep(6));
    BOOST_CHECK(!fixedParticle.isAsleep());
}

BOOST_AUTO_TEST_CASE(test_sleep_reset_clears_state)
{
    disableLogFileOutput();

    Particle particle(Eigen::Vector3d::Zero(), Eigen::Vector3d::Zero(), 1.0, 7);

    MockScene scene;
    scene.bodies.push_back(SolverBodyHandle{.entity = &particle, .inverseMass = 1.0});

    SleepConfig config{};
    config.enabled = true;
    config.frozenFrameCount = 1;

    KineticSleepStrategy strategy(config);

    runOneRenderFrame(strategy, scene);
    BOOST_CHECK(strategy.isAsleep(7));

    strategy.reset();
    BOOST_CHECK(!strategy.isAsleep(7));
}
