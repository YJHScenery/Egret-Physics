/**
 * @file        offline_simulation_runner.cpp
 * @brief       离线求解器实现：驱动 WorldSceneManager 求解并按抽帧写入 EPSX 文件。
 * @details     使用与实时模拟完全相同的策略组合（Velocity Verlet 积分、八叉树广相、
 *              有摩擦接触解算、动能休眠），保证离线结果与实时预览在同样步长下一致。
 *
 * @author      jehor
 * @date        2026-10-02
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Simulator
 * @defgroup    Offline (离线求解)
 */

#include "offline_simulation_runner.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "broad_phase_strategy/octree_broad_strategy.h"
#include "constraints_base.h"
#include "contact_strategy/frictional_contact_resolver.h"
#include "egret_version.h"
#include "epsx_writer.h"
#include "field_base.h"
#include "integrator_strategy/velocity_verlet_integrator.h"
#include "kinetic_sleep_strategy.h"
#include "physical_entity.h"
#include "scene_manager.h"
#include "sleep_config.h"
#include "solver.h"
#include "world_scene_manager.h"

namespace egret
{
    namespace
    {
        /** 把错误描述写入调用方提供的指针（为空则忽略）。 */
        void setError(QString* errorMessage, const QString& message)
        {
            if (errorMessage != nullptr) {
                *errorMessage = message;
            }
        }

        /** 组装生成者描述字符串。 */
        std::string buildGeneratorString()
        {
            return std::string{"Egret Physics "} + std::string{EGRET_VERSION_STRING};
        }

        /**
         * @brief  采集当前世界状态为一帧记录。
         * @param  world 世界管理器。
         * @param  frameIndex 帧号。
         * @param  stepIndex 对应的求解器步号。
         * @param  simTimeNs 对应的仿真时刻（纳秒）。
         * @return 帧记录。
         */
        epsx::FrameState snapshotFrame(WorldSceneManager& world,
                                      const std::uint32_t frameIndex,
                                      const std::uint64_t stepIndex,
                                      const std::uint64_t simTimeNs)
        {
            epsx::FrameState frame{};
            frame.frameIndex = frameIndex;
            frame.stepIndex = stepIndex;
            frame.simTimeNs = simTimeNs;

            const std::vector<PhysicalEntity*> entities = world.getAllPhysicalEntities();
            frame.bodies.reserve(entities.size());

            for (std::size_t slot = 0; slot < entities.size(); ++slot) {
                const PhysicalEntity* entity = entities[slot];
                if (entity == nullptr) {
                    continue;
                }

                epsx::BodyState body{};
                body.id = entity->getId();
                body.slot = static_cast<std::uint32_t>(slot);
                body.position = entity->getPositionCR();
                body.rotation = entity->getTransformCR().getRotation();
                body.linearVelocity = entity->getSpeedCR();
                body.angularVelocity = entity->getAngularCR();
                body.mass = entity->getMass();
                body.restitution = entity->getRestitution();

                std::uint32_t flags{0};
                if (entity->isAsleep()) {
                    flags |= epsx::kBodyStateAsleep;
                }
                if (!entity->isFixed()) {
                    flags |= epsx::kBodyStateIntegration;
                }
                else {
                    flags |= epsx::kBodyStateStatic;
                }
                flags |= epsx::kBodyStateCollision;
                body.stateFlags = flags;

                frame.bodies.push_back(body);
            }

            const std::vector<ConstraintsBase*> constraints = world.getAllConstraints();
            frame.constraints.reserve(constraints.size());
            for (std::size_t slot = 0; slot < constraints.size(); ++slot) {
                ConstraintsBase* constraint = constraints[slot];
                if (constraint == nullptr) {
                    continue;
                }

                epsx::ConstraintState state{};
                state.id = constraint->getId();
                state.slot = static_cast<std::uint32_t>(slot);
                state.type = static_cast<std::uint32_t>(constraint->getType());
                state.flags = constraint->isEnabled() ? epsx::kConstraintFlagEnabled : 0u;
                state.restLength = constraint->getReferenceLength();
                state.error = constraint->computeConstraintError();
                state.currentLength = state.restLength + state.error;

                // 约束关联的实体集合是无序集合，这里按 ID 升序取前两个，
                // 保证同一场景每次导出的结果字节一致。
                std::vector<std::uint64_t> entityIds;
                entityIds.reserve(constraint->getConstrainedEntities().size());
                for (const PhysicalEntity* entity : constraint->getConstrainedEntities()) {
                    if (entity != nullptr) {
                        entityIds.push_back(entity->getId());
                    }
                }
                std::sort(entityIds.begin(), entityIds.end());
                if (!entityIds.empty()) {
                    state.entityAId = entityIds[0];
                }
                if (entityIds.size() >= 2) {
                    state.entityBId = entityIds[1];
                }

                frame.constraints.push_back(state);
            }

            const std::vector<FieldBase*> fields = world.getAllFields();
            frame.fields.reserve(fields.size());
            for (std::size_t slot = 0; slot < fields.size(); ++slot) {
                const FieldBase* field = fields[slot];
                if (field == nullptr) {
                    continue;
                }

                epsx::FieldState state{};
                state.id = field->getId();
                state.slot = static_cast<std::uint32_t>(slot);
                state.type = static_cast<std::uint32_t>(field->getType());
                state.flags = epsx::kFieldFlagEnabled;
                frame.fields.push_back(state);
            }

            return frame;
        }

        /**
         * @brief  构造场景描述块 JSON。
         * @details 在编辑器导出的场景 JSON 基础上追加 EPSX 元信息（采样、求解器配置、
         *          帧段布局与实体表），使文件自包含。
         * @param  sceneJson 编辑器导出的场景 JSON。
         * @param  info 写入器的静态信息。
         * @param  solverConfig 求解器运行时配置（迭代次数、穿透容差等）。
         * @param  world 世界管理器（用于导出实体表）。
         * @param  frameCount 预计帧数。
         * @param  durationNs 声明总时长（纳秒）。
         * @return 紧凑 JSON 文本。
         */
        std::string buildSceneDescription(const QString& sceneJson,
                                          const epsx::EpsxWriter::SceneInfo& info,
                                          const SolverConfig& solverConfig,
                                          WorldSceneManager& world,
                                          const std::uint64_t frameCount,
                                          const std::uint64_t durationNs,
                                          const std::uint64_t decimation)
        {
            QJsonObject root = QJsonDocument::fromJson(sceneJson.toUtf8()).object();

            root[QStringLiteral("format")] = QStringLiteral("EPSX");
            root[QStringLiteral("formatVersion")] = QStringLiteral("1.0");
            root[QStringLiteral("generator")] = QString::fromStdString(info.generator);

            QJsonObject sampling{};
            sampling[QStringLiteral("solverStepNs")] = static_cast<double>(info.solverStepNs);
            sampling[QStringLiteral("intervalNs")] = static_cast<double>(info.sampleIntervalNs);
            sampling[QStringLiteral("decimation")] = static_cast<double>(decimation);
            sampling[QStringLiteral("frameCount")] = static_cast<double>(frameCount);
            sampling[QStringLiteral("durationNs")] = static_cast<double>(durationNs);
            root[QStringLiteral("sampling")] = sampling;

            QJsonArray sections{};
            if (info.bodyCount > 0) {
                QJsonObject section{};
                section[QStringLiteral("type")] = QStringLiteral("BODY");
                section[QStringLiteral("recordVersion")] = 1;
                section[QStringLiteral("strideBytes")] = static_cast<int>(epsx::kBodyStrideFloat64);
                section[QStringLiteral("count")] = static_cast<int>(info.bodyCount);
                sections.append(section);
            }
            if (info.constraintCount > 0) {
                QJsonObject section{};
                section[QStringLiteral("type")] = QStringLiteral("CONS");
                section[QStringLiteral("recordVersion")] = 1;
                section[QStringLiteral("strideBytes")] = static_cast<int>(epsx::kConstraintStrideFloat64);
                section[QStringLiteral("count")] = static_cast<int>(info.constraintCount);
                sections.append(section);
            }
            if (info.fieldCount > 0) {
                QJsonObject section{};
                section[QStringLiteral("type")] = QStringLiteral("FLDS");
                section[QStringLiteral("recordVersion")] = 1;
                section[QStringLiteral("strideBytes")] = static_cast<int>(epsx::kFieldStrideFloat64);
                section[QStringLiteral("count")] = static_cast<int>(info.fieldCount);
                sections.append(section);
            }
            root[QStringLiteral("frameSections")] = sections;

            const std::vector<PhysicalEntity*> entities = world.getAllPhysicalEntities();
            const std::vector<std::string> names = world.getBodyNames();
            QJsonArray entityTable{};
            for (std::size_t slot = 0; slot < entities.size(); ++slot) {
                const PhysicalEntity* entity = entities[slot];
                if (entity == nullptr) {
                    continue;
                }

                QJsonObject entry{};
                entry[QStringLiteral("slot")] = static_cast<int>(slot);
                entry[QStringLiteral("id")] = static_cast<double>(entity->getId());
                if (slot < names.size()) {
                    entry[QStringLiteral("name")] = QString::fromStdString(names[slot]);
                }
                entry[QStringLiteral("mass")] = entity->getMass();
                entry[QStringLiteral("restitution")] = entity->getRestitution();
                entry[QStringLiteral("fixed")] = entity->isFixed();
                entityTable.append(entry);
            }
            root[QStringLiteral("entities")] = entityTable;

            QJsonObject solver{};
            solver[QStringLiteral("integrator")] = QStringLiteral("velocity_verlet");
            solver[QStringLiteral("broadPhase")] = QStringLiteral("octree");
            solver[QStringLiteral("contactResolver")] = QStringLiteral("frictional");
            solver[QStringLiteral("velocityIterations")] = static_cast<int>(solverConfig.velocityIterations);
            solver[QStringLiteral("positionIterations")] = static_cast<int>(solverConfig.positionIterations);
            solver[QStringLiteral("penetrationSlop")] = solverConfig.penetrationSlop;
            solver[QStringLiteral("contactAlgorithm")] = QStringLiteral("contact_graph_v1");
            solver[QStringLiteral("graphIterations")] = static_cast<int>(solverConfig.graphIterations);
            solver[QStringLiteral("positionTolerance")] = solverConfig.positionTolerance;
            solver[QStringLiteral("maxAngularCorrectionPerStep")] = solverConfig.maxAngularCorrectionPerStep;
            solver[QStringLiteral("restitutionVelocityThreshold")] = solverConfig.restitutionVelocityThreshold;
            solver[QStringLiteral("positionCorrectionBeta")] = solverConfig.positionCorrectionBeta;
            root[QStringLiteral("solver")] = solver;

            return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)).toStdString();
        }
    }

    bool OfflineSimulationRunner::run(const Request& request, Report* report, QString* errorMessage)
    {
        if (request.sceneJson.trimmed().isEmpty()) {
            setError(errorMessage, QStringLiteral("场景 JSON 为空"));
            return false;
        }
        if (request.outputPath.trimmed().isEmpty()) {
            setError(errorMessage, QStringLiteral("输出路径为空"));
            return false;
        }
        if (request.durationSeconds <= 0.0) {
            setError(errorMessage, QStringLiteral("求解时长必须大于 0"));
            return false;
        }

        const std::optional<std::uint64_t> decimation =
            epsx::decimationFor(request.solverStepNs, request.sampleIntervalNs);
        if (!decimation.has_value()) {
            setError(errorMessage, QStringLiteral("采样间隔必须是求解步长的整数倍"));
            return false;
        }

        const std::uint64_t durationNs = epsx::secondsToNanoseconds(request.durationSeconds);
        const std::uint64_t totalSteps = durationNs / request.solverStepNs;
        const std::uint64_t frameCount = durationNs / request.sampleIntervalNs + 1;

        if (totalSteps == 0) {
            setError(errorMessage, QStringLiteral("求解时长不足一个推进步长"));
            return false;
        }

        // ---- 1. 重建场景 ----
        SceneRecord record = SceneManagerViewModel::createSceneFromJsonString(request.sceneJson);
        if (record.bodies.empty()) {
            setError(errorMessage, QStringLiteral("场景中没有可用实体，或场景 JSON 无法解析"));
            return false;
        }

        auto solver = std::make_unique<Solver>(
            request.solverConfig,
            std::make_unique<VelocityVerletIntegratorStrategy>(),
            std::make_unique<OctreeBroadStrategy>(),
            std::make_unique<FrictionalContactResolverStrategy>());
        solver->setSleepStrategy(std::make_unique<KineticSleepStrategy>(SleepConfig{}));

        auto world = std::make_unique<WorldSceneManager>(std::move(solver));
        world->registerScene(record);

        const std::vector<PhysicalEntity*> entities = world->getAllPhysicalEntities();
        const std::vector<ConstraintsBase*> constraints = world->getAllConstraints();
        const std::vector<FieldBase*> fields = world->getAllFields();

        // ---- 2. 打开输出文件 ----
        epsx::EpsxWriter::SceneInfo info{};
        info.generator = buildGeneratorString();
        info.solverStepNs = request.solverStepNs;
        info.sampleIntervalNs = request.sampleIntervalNs;
        info.durationNs = frameCount * request.sampleIntervalNs;
        info.bodyCount = static_cast<std::uint32_t>(entities.size());
        info.constraintCount = static_cast<std::uint32_t>(constraints.size());
        info.fieldCount = static_cast<std::uint32_t>(fields.size());
        info.sceneJson = buildSceneDescription(request.sceneJson, info, request.solverConfig, *world,
                                               frameCount, durationNs, decimation.value());

        epsx::EpsxWriter writer(request.outputPath);
        if (!writer.open(info, errorMessage)) {
            return false;
        }

        QElapsedTimer timer{};
        timer.start();

        // ---- 3. 第 0 帧为初始状态 ----
        if (!writer.writeFrame(snapshotFrame(*world, 0, 0, 0), errorMessage)) {
            return false;
        }

        // ---- 4. 求解并按抽帧比记录 ----
        const double stepSeconds = epsx::nanosecondsToSeconds(request.solverStepNs);
        const std::uint64_t k = decimation.value();

        std::uint64_t performedSteps{0};
        bool canceled{false};

        for (std::uint64_t step = 1; step <= totalSteps; ++step) {
            std::ignore = world->tick(stepSeconds);
            performedSteps = step;

            if (step % 100 == 0) {
                if (request.progressCallback) {
                    request.progressCallback(step, totalSteps);
                }
                // 与进度回调同频检查取消请求，避免每步都调用回调
                if (request.shouldCancel && request.shouldCancel()) {
                    canceled = true;
                    break;
                }
            }

            if (step % k != 0) {
                continue;
            }

            const std::uint64_t frameIndex = step / k;
            if (frameIndex >= frameCount) {
                break;
            }

            if (!writer.writeFrame(snapshotFrame(*world,
                                                 static_cast<std::uint32_t>(frameIndex),
                                                 step,
                                                 frameIndex * request.sampleIntervalNs),
                                   errorMessage)) {
                return false;
            }
        }

        // ---- 5. 收尾 ----
        if (!writer.close(errorMessage)) {
            return false;
        }

        if (request.progressCallback && !canceled) {
            request.progressCallback(totalSteps, totalSteps);
        }

        if (report != nullptr) {
            report->stepCount = performedSteps;
            report->frameCount = writer.frameCount();
            report->canceled = canceled;
            report->wallClockSeconds = static_cast<double>(timer.elapsed()) / 1000.0;
            report->bodyCount = info.bodyCount;
            report->constraintCount = info.constraintCount;
            report->fieldCount = info.fieldCount;
            const QFileInfo fileInfo(request.outputPath);
            report->fileBytes = static_cast<std::uint64_t>(fileInfo.size());
        }

        return true;
    }

    QString OfflineSimulationRunner::defaultSimulationFilePath()
    {
        const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        return baseDir + QStringLiteral("/simulations/egret_simulation.epsx");
    }
}
