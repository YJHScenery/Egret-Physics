/** @file test_scene_control_world.cpp @brief 在实际世界和 Verlet 求解器中验证控制动作及安全边界。 */
#include "world_scene_manager.h"
#include "shape_box.h"
#include "control/predicate_program.h"
#include "velocity_verlet_integrator.h"
#include "bvh_broad_strategy.h"
#include "frictional_contact_resolver.h"
#include "connecting_rod.h"
#include <array>
#include <cmath>
#include <stdexcept>

namespace
{
    /** @brief 为控制集成测试创建真实物理世界。 */
    std::unique_ptr<egret::WorldSceneManager> makeWorld()
    {
        return std::make_unique<egret::WorldSceneManager>(std::make_unique<egret::Solver>(egret::SolverConfig{},
            std::make_unique<egret::VelocityVerletIntegratorStrategy>(),std::make_unique<egret::BVHBroadStrategy>(),
            std::make_unique<egret::FrictionalContactResolverStrategy>()));
    }
    /** @brief 执行始终启用的集成断言。 */
    void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
    /** @brief 创建从指定时间开始首次触发的规则。 */
    egret::control::Rule at(double time,std::vector<egret::control::Command> commands)
    {
        egret::control::Predicate p;p.kind=egret::control::PredicateKind::Time;
        p.comparison=egret::control::Comparison::GreaterEqual;p.lower=time;
        return {{p},std::move(commands),egret::control::Trigger::Once};
    }

    /** @brief 验证旧回放遗漏辅助锚点、原始实体暂缺与动态实体跳帧恢复的生命周期。 */
    void testPlaybackBodyLifetime()
    {
        using namespace egret;
        auto world = makeWorld();
        SceneRecord scene;
        BodyRecord anchor;
        anchor.entity = std::make_shared<Particle>(Eigen::Vector3d::Zero(), Eigen::Vector3d::Zero(), 0, 99);
        anchor.visible = false;
        anchor.enableCollision = false;
        anchor.enableIntegration = false;
        anchor.auxiliaryAnchor = true;
        BodyRecord first;
        first.entity = std::make_shared<Particle>(Eigen::Vector3d(1,0,0), Eigen::Vector3d::Zero(), 1, 1);
        first.entity->setShape(std::make_shared<ShapeBox>(1,1,1));
        BodyRecord second;
        second.entity = std::make_shared<Particle>(Eigen::Vector3d(2,0,0), Eigen::Vector3d::Zero(), 1, 2);
        second.entity->setShape(std::make_shared<ShapeBox>(1,1,1));
        scene.bodies = {anchor, first, second};
        scene.constraints.push_back({"固定支点", std::make_shared<ConnectingRod>(1, anchor.entity.get(), first.entity.get(), 100)});
        scene.constraints.push_back({"实体连接", std::make_shared<ConnectingRod>(1, first.entity.get(), second.entity.get(), 101)});
        const std::weak_ptr<PhysicalEntity> anchorLifetime = anchor.entity;
        const std::weak_ptr<PhysicalEntity> secondLifetime = second.entity;
        const auto* secondPointer = second.entity.get();
        world->registerScene(scene);
        scene = {};
        anchor.entity.reset(); first.entity.reset(); second.entity.reset();

        const std::array<std::uint64_t,1> firstFrame{1};
        world->synchronizePlaybackBodies(firstFrame);
        check(!anchorLifetime.expired() && world->getBodyCount() == 2, "回放误删未记录的辅助锚点");
        check(world->isConstraintActive(100) && world->buildConstraintRenderItems().size() == 1,
              "辅助锚点约束无法渲染，或缺席实体约束仍被启用");
        check(!secondLifetime.expired() && !world->isBodyActive(2), "缺席原始实体引用失效或仍处于活动状态");
        check(world->buildRenderItems().size() == 1, "缺席原始实体仍被渲染");

        const std::array<std::uint64_t,2> fullFrame{1,2};
        world->synchronizePlaybackBodies(fullFrame);
        check(world->restoreControlBody(2,2), "原始实体无法在跳帧后恢复");
        bool restoredPointer = false;
        for (const auto* entity : world->getAllPhysicalEntities())
            if (entity->getId() == 2) restoredPointer = entity == secondPointer;
        check(restoredPointer && world->isConstraintActive(101)
              && world->buildConstraintRenderItems().size() == 2, "恢复后约束没有保持原实体指针身份");
        const std::uint64_t dynamicId = (std::uint64_t{1} << 63) | 7;
        check(world->restoreControlBody(dynamicId,1), "动态实体无法按模板恢复");
        check(world->getBodyCount() == 4, "动态实体恢复数量错误");
        world->synchronizePlaybackBodies(fullFrame);
        check(world->getBodyCount() == 3 && world->controlBodyTemplate(dynamicId) == 0, "动态实体没有随缺席帧移除");
        check(world->restoreControlBody(dynamicId,1), "动态实体无法在倒放时再次恢复");
        world->clear();
        check(anchorLifetime.expired() && secondLifetime.expired(), "清空世界未释放回放保留实体");
    }
}

/** @brief 验证持久力、激活、退出求解、恢复以及无效批次不产生部分修改。 */
void testSceneControlWorld()
{
    testPlaybackBodyLifetime();
    using namespace egret;
    using namespace egret::control;
    auto world=makeWorld();
    auto body=std::make_shared<Particle>(Eigen::Vector3d::Zero(),Eigen::Vector3d::Zero(),2,1);
    auto added=std::make_shared<Particle>(Eigen::Vector3d(10,0,0),Eigen::Vector3d::Zero(),1,2);
    body->setShape(std::make_shared<ShapeBox>(1,1,1));added->setShape(std::make_shared<ShapeBox>(1,1,1));
    SceneRecord scene;
    BodyRecord first;first.entity=body;
    BodyRecord second;second.entity=added;second.controlEnabled=false;
    scene.bodies={first,second};world->registerScene(scene);
    world->setControlProgram(std::make_unique<PredicateProgram>(std::vector<Rule>{
        at(0,{{CommandKind::SetForce,1,{4,0,0}}}),
        at(0.1,{{CommandKind::SetActive,2,{},true},{CommandKind::SetVelocity,2,{3,0,0}}}),
        at(0.2,{{CommandKind::SetActive,1,{},false}}),
        at(0.3,{{CommandKind::SetActive,1,{},true},{CommandKind::SetForce,1,{0,0,0}}})}));
    const auto fieldCount=world->getAllFields().size();
    (void)world->tick(0.1);
    check(std::abs(body->getVelocity().x()-0.2)<1e-12 && std::abs(body->getPosition().x()-0.01)<1e-12,
          "控制力没有参与 Verlet 的两次力求值");
    check(!world->isBodyActive(2) && added->getPosition().x()==10,"预创建的停用物体提前参与求解");
    (void)world->tick(0.1);
    check(world->isBodyActive(2) && std::abs(added->getPosition().x()-10.3)<1e-12,"物体激活或设置速度失败");
    const auto position=body->getPosition();(void)world->tick(0.1);
    check(!world->isBodyActive(1) && body->getPosition()==position,"停用物体仍在积分");
    const auto handles=world->getBodies();
    for (const auto& handle:handles) if(handle.entity==body.get())
        check(!handle.enableCollision && !handle.enableIntegration,"停用物体仍参与碰撞或积分");
    (void)world->tick(0.1);
    check(world->isBodyActive(1) && std::abs(body->getVelocity().x()-0.4)<1e-12,"恢复物体或取消控制力失败");
    check(world->getAllFields().size()==fieldCount,"控制力动作改变了文件的静态场表长度");
    world->setControlProgram(std::make_unique<PredicateProgram>(std::vector<Rule>{at(0,
        {{CommandKind::SetVelocity,1,{100,0,0}},{CommandKind::SetVelocity,999,{1,0,0}}})}));
    const double time=world->getSimulationTime();const auto velocity=body->getVelocity();
    bool rejected=false;try{(void)world->tick(0.1);}catch(const std::runtime_error&){rejected=true;}
    check(rejected && body->getVelocity()==velocity && world->getSimulationTime()==time,"无效动作批次部分修改了世界");

    auto contactWorld=makeWorld();
    auto ground=std::make_shared<Particle>(Eigen::Vector3d(0,0,-0.5),Eigen::Vector3d::Zero(),0,10);
    auto resting=std::make_shared<Particle>(Eigen::Vector3d(0,0,0.5),Eigen::Vector3d::Zero(),1,11);
    ground->setShape(std::make_shared<ShapeBox>(10,10,1));resting->setShape(std::make_shared<ShapeBox>(1,1,1));
    SceneRecord contactScene;BodyRecord g;g.entity=ground;BodyRecord r;r.entity=resting;contactScene.bodies={g,r};
    contactWorld->registerScene(contactScene);
    Predicate touching;touching.kind=PredicateKind::Contact;touching.first=10;touching.second=11;
    Rule onContact{{touching},{{CommandKind::SetVelocity,11,{2,0,0}}},Trigger::Once};
    contactWorld->setControlProgram(std::make_unique<PredicateProgram>(std::vector<Rule>{onContact}));
    (void)contactWorld->tick(0.01);check(resting->getVelocity().x()==0,"接触事件在首步之前被伪造");
    (void)contactWorld->tick(0.01);check(resting->getVelocity().x()==2,"上一计算步的真实接触未传入控制程序");
}
