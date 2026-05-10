//
// 由 GitHub Copilot 于 2026/4/23 创建。
//

#include "scene_manager.h"

// 原代码：
// namespace egret {
//
// class SceneManager {
//
// };
//
// } // egret
//
// 新代码说明：
// 1. 将原来的空壳类替换为可直接暴露给 QML 的 ViewModel 门面对象。
// 2. 该对象负责驱动模型层世界管理器、维护固定步长累积，并刷新 QML 渲染数据。

#include <algorithm>

#include "world_scene_manager.h"
#include "broad_phase_strategy/brute_force_broad_phase.h"
#include "contact_strategy/frictionless_contact_resolver.h"
#include "integrator_strategy/semi_implicit_euler_integrator.h"
#include "shape_cylinder.h"
#include "shape_cylindrical_shell.h"
#include "shape_disk.h"
#include "shape_ring.h"
#include "shape_rod.h"
#include "shape_spherical_shell.h"

namespace egret
{
    SceneManagerViewModel::SceneManagerViewModel(QObject* parent): QObject(parent), m_bodyModel(this)
    {
        m_timer.setInterval(16);
        m_timer.setTimerType(Qt::PreciseTimer);
        connect(&m_timer, &QTimer::timeout, this, &SceneManagerViewModel::onFrameTick);

        rebuildDemoWorld();
        refreshBodyModel();
        m_frameClock.start();
    }

    bool SceneManagerViewModel::isRunning() const
    {
        return m_running;
    }

    double SceneManagerViewModel::simTime() const
    {
        return m_simTime;
    }

    quint64 SceneManagerViewModel::stepCount() const
    {
        return m_stepCount;
    }

    int SceneManagerViewModel::entityCount() const
    {
        return m_entityCount;
    }

    double SceneManagerViewModel::fps() const
    {
        return m_fps;
    }

    double SceneManagerViewModel::fixedStepMs() const
    {
        return m_fixedStepSeconds * 1000.0;
    }

    SceneBodyModel* SceneManagerViewModel::bodyModel()
    {
        return &m_bodyModel;
    }

    void SceneManagerViewModel::play()
    {
        if (m_running) {
            return;
        }

        setRunning(true);
        m_frameClock.restart();
        m_timer.start();
    }

    void SceneManagerViewModel::pause()
    {
        if (!m_running) {
            return;
        }

        m_timer.stop();
        setRunning(false);
    }

    void SceneManagerViewModel::toggleRunning()
    {
        if (m_running) {
            pause();
        } else {
            play();
        }
    }

    void SceneManagerViewModel::stepOnce()
    {
        advanceFixedStep(true);
    }

    void SceneManagerViewModel::reset()
    {
        pause();
        rebuildDemoWorld();
        refreshBodyModel();

        m_simTime = 0.0;
        m_stepCount = 0;
        m_accumulator = 0.0;
        m_entityCount = m_world != nullptr ? static_cast<int>(m_world->getBodyCount()) : 0;

        emit simTimeChanged();
        emit stepCountChanged();
        emit entityCountChanged();
        emit sceneReset();
        emit entityChanged();
    }

    void SceneManagerViewModel::spawnSphere()
    {
        if (m_world == nullptr) {
            return;
        }

        const std::uint64_t id = m_world->spawnSphere(
            "动态球体",
            {180.0 + static_cast<double>(m_world->getBodyCount()) * 28.0, 120.0, 0.0},
            {40.0, 0.0, 0.0},
            28.0,
            1.0);

        static_cast<void>(id);
        refreshBodyModel();
        m_entityCount = static_cast<int>(m_world->getBodyCount());
        emit entityCountChanged();
        emit entityChanged();
    }

    void SceneManagerViewModel::spawnBox()
    {
        if (m_world == nullptr) {
            return;
        }

        const std::uint64_t id = m_world->spawnBox(
            "动态盒体",
            {420.0 + static_cast<double>(m_world->getBodyCount()) * 22.0, 90.0, 0.0},
            {0.0, 0.0, 0.0},
            {52.0, 52.0, 1.0},
            2.0);

        static_cast<void>(id);
        refreshBodyModel();
        m_entityCount = static_cast<int>(m_world->getBodyCount());
        emit entityCountChanged();
        emit entityChanged();
    }

    void SceneManagerViewModel::onFrameTick()
    {
        const double realFrameSeconds = std::max(0.0, static_cast<double>(m_frameClock.restart()) / 1000.0);
        m_accumulator += realFrameSeconds;
        updateFps(realFrameSeconds);

        int subStepCount = 0;
        while (m_accumulator >= m_fixedStepSeconds && subStepCount < m_maxSubSteps) {
            advanceFixedStep(false);
            m_accumulator -= m_fixedStepSeconds;
            ++subStepCount;
        }

        if (m_accumulator >= m_fixedStepSeconds * static_cast<double>(m_maxSubSteps)) {
            m_accumulator = 0.0;
        }
    }

    void SceneManagerViewModel::rebuildDemoWorld()
    {
        SolverConfig config{};
        config.lockToXYPlane = true;

        auto solver = std::make_unique<Solver>(
            config,
            std::make_unique<SemiImplicitEulerIntegratorStrategy>(),
            std::make_unique<BruteForceBroadPhaseStrategy>(),
            std::make_unique<FrictionlessContactResolverStrategy>());

        m_world = std::make_unique<WorldSceneManager>(std::move(solver));
        m_world->clear();

        m_world->addGravityField({0.0, 180.0, 0.0}, {0.0, 0.0, 0.0}, "重力场");

        m_world->spawnBox("地面", {420.0, 400.0, 0.0}, {0.0, 0.0, 0.0}, {760.0, 28.0, 1.0}, 0.0);
        m_world->spawnSphere("小球 A", {220.0, 60.0, 0.0}, {0.0, 0.0, 0.0}, 28.0, 10.0);


        m_entityCount = static_cast<int>(m_world->getBodyCount());
    }

    void SceneManagerViewModel::refreshBodyModel()
    {
        if (m_world == nullptr) {
            m_bodyModel.clear();
            return;
        }

        std::vector<SceneBodyVisualItem> items;
        const std::vector<SceneRenderItem> renderItems = m_world->buildRenderItems();
        items.reserve(renderItems.size());

        for (const SceneRenderItem& renderItem : renderItems) {
            SceneBodyVisualItem item{};
            item.id = renderItem.id;
            item.kind = QString::fromStdString(renderItem.kind);
            item.x = renderItem.x;
            item.y = renderItem.y;
            item.width = renderItem.width;
            item.height = renderItem.height;
            item.color = QColor(QString::fromStdString(renderItem.color));
            item.label = QString::fromStdString(renderItem.label);
            items.push_back(std::move(item));
        }

        m_bodyModel.setItems(items);
    }

    void SceneManagerViewModel::advanceFixedStep(const bool emitFrameSignal)
    {
        if (m_world == nullptr) {
            return;
        }

        const SolverStepResult result = m_world->tick(m_fixedStepSeconds);
        std::ignore = result;

        m_simTime = m_world->getSimulationTime();
        m_stepCount = m_world->getStepCount();
        m_entityCount = static_cast<int>(m_world->getBodyCount());

        refreshBodyModel();
        emit simTimeChanged();
        emit stepCountChanged();
        emit entityCountChanged();
        emit entityChanged();
        if (emitFrameSignal) {
            emit frameAdvanced();
        }
    }

    void SceneManagerViewModel::updateFps(const double realFrameSeconds)
    {
        if (realFrameSeconds <= 0.0) {
            return;
        }

        const double newFps = 1.0 / realFrameSeconds;
        m_fps = m_fps <= 0.0 ? newFps : m_fps * 0.9 + newFps * 0.1;
        emit fpsChanged();
    }

    void SceneManagerViewModel::setRunning(const bool running)
    {
        if (m_running == running) {
            return;
        }

        m_running = running;
        emit runningChanged();
    }
}
