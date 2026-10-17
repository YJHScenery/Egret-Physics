/**
 * @file test_contact_graph.cpp
 * @brief 接触图、稳定支撑、恢复与摩擦的独立回归测试。
 */
#include "contact_graph_projector.h"
#include "contact_manifold_builder.h"
#include "frictional_contact_resolver.h"
#include "rigid_body.h"
#include "shape_box.h"
#include "shape_cylinder.h"
#include "particle.h"
#include "connecting_rod.h"
#include "kinetic_sleep_strategy.h"
#include "async_logger.h"
#include "solver.h"
#include "gravity_field.h"
#include "air_resistance.h"
#include "brute_force_broad_strategy.h"
#include "velocity_verlet_integrator.h"
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>

using namespace egret;

/** @brief 为接触回归提供最小场景，避免依赖 GUI 世界管理器。 */
class ContactTestScene final : public SolverSceneSnapshotBase
{
public:
    std::vector<SolverBodyHandle> bodies;
    std::vector<FieldBase*> fields;
    std::vector<ConstraintsBase*> joints;
    mutable std::vector<const ConstraintsBase*> constJoints;
    /** @brief 返回可变实体视图。 */
    std::span<SolverBodyHandle> getBodies() override { return bodies; }
    /** @brief 返回只读实体视图。 */
    std::span<const SolverBodyHandle> getBodies() const override { return bodies; }
    /** @brief 此测试没有力场。 */
    std::span<FieldBase*> getFields() override { return fields; }
    /** @brief 此测试没有力场。 */
    std::span<FieldBase* const> getFields() const override { return fields; }
    /** @brief 此测试没有外部约束。 */
    std::span<ConstraintsBase*> getConstraints() override { return joints; }
    /** @brief 此测试没有外部约束。 */
    std::span<const ConstraintsBase*> getConstraints() const override
    {
        constJoints.assign(joints.begin(),joints.end());
        return constJoints;
    }
    /** @brief 返回固定时钟。 */
    double getSimulationTime() const override { return 0.0; }
    /** @brief 投影测试不推进时间。 */
    void advanceSimulationTime(double) override {}
};

/** @brief 始终启用的检查，Release 下也不会被移除。 */
void require(bool passed, const char* message)
{
    if (!passed) throw std::runtime_error(message);
}

/** @brief 创建根据质量和形状计算的求解句柄。 */
SolverBodyHandle handle(PhysicalEntity& body)
{
    SolverBodyHandle result;
    result.entity = &body;
    result.inverseMass = body.getMass()>0.0 ? 1.0/body.getMass() : 0.0;
    if (result.inverseMass>0.0 && body.hasRotation())
        result.inverseInertiaTensor = body.getShape()->getInertiaTensor(body.getMass()).inverse();
    return result;
}

/** @brief 验证偏心平面接触仍给出竖直法线和覆盖支撑面，并满足交换对称性。 */
void testManifolds()
{
    RigidBody ground(0.0,1), box(10.0,2), cylinder(10.0,3);
    ground.setShape(std::make_shared<ShapeBox>(10000,10000,130));
    box.setShape(std::make_shared<ShapeBox>(100,100,100));
    box.setPosition({300,400,115});
    cylinder.setShape(std::make_shared<ShapeCylinder>(100,200));
    cylinder.setPosition({-100,-100,165});
    for (auto* body : {&box,&cylinder}) {
        const auto points = buildContactManifold(ground,*body,0.0,Eigen::Vector3d::Zero(),0.0,Eigen::Vector3d::Zero());
        const auto reverse = buildContactManifold(*body,ground,0.0,Eigen::Vector3d::Zero(),0.0,Eigen::Vector3d::Zero());
        require(points.size()>=4 && reverse.size()==points.size(),"面接触流形缺失");
        for (const auto& p : points) require((p.normal-Eigen::Vector3d::UnitZ()).norm()<1e-12,"偏心面接触法线错误");
        for (const auto& p : reverse) require((p.normal+Eigen::Vector3d::UnitZ()).norm()<1e-12,"交换实体法线未翻转");
    }
    box.setPosition({300,400,120});
    const auto separated = buildContactManifold(ground,box,0.0,Eigen::Vector3d::Zero(),0.0,Eigen::Vector3d::Zero());
    require(separated.empty() || separated.front().normal.norm()<0.5,"分离实体产生面约束");
}

/** @brief 验证三体环保留全部约束，置换、重复边不会改变质量加权解。 */
void testGraph()
{
    Particle a(Eigen::Vector3d::Zero(), Eigen::Vector3d::Zero(), 1.0,1), b(Eigen::Vector3d::Zero(), Eigen::Vector3d::Zero(), 2.0,2), c(Eigen::Vector3d::Zero(), Eigen::Vector3d::Zero(), 3.0,3);
    ContactTestScene scene;
    scene.bodies = {handle(a),handle(b),handle(c)};
    const Eigen::Vector3d na(1,0,0), nb(-0.5,std::sqrt(0.75),0), nc(-0.5,-std::sqrt(0.75),0);
    std::vector<SolverContactConstraint> contacts;
    for (int k=0;k<3;++k) {
        SolverContactConstraint row;
        row.bodyAIndex=k; row.bodyBIndex=(k+1)%3;
        row.contactNormal = k==0 ? na : k==1 ? nb : nc;
        row.penetration=0.03;
        contacts.push_back(row);
    }
    SolverConfig config;
    config.positionIterations=1; config.positionCorrectionBeta=1; config.penetrationSlop=0;
    config.graphIterations=4096; config.positionTolerance=1e-10;
    SolverStats stats;
    ContactGraphProjector::project(scene.bodies,contacts,config,stats);
    const std::vector<Eigen::Vector3d> result{a.getPosition(),b.getPosition(),c.getPosition()};
    require(stats.contactIslandCount==1,"闭环被拆开");
    require(stats.maxResidualPenetration<1e-8,"闭环未恢复穿透");
    require((result[0]+2*result[1]+3*result[2]).norm()<1e-12,"内部修正改变质量中心");
    a.setPosition(Eigen::Vector3d::Zero()); b.setPosition(Eigen::Vector3d::Zero()); c.setPosition(Eigen::Vector3d::Zero());
    std::reverse(contacts.begin(),contacts.end());
    contacts.push_back(contacts.front());
    stats={};
    ContactGraphProjector::project(scene.bodies,contacts,config,stats);
    require((a.getPosition()-result[0]).norm()<1e-8 && (b.getPosition()-result[1]).norm()<1e-8 &&
            (c.getPosition()-result[2]).norm()<1e-8,"置换或重复边改变投影解");
}

/** @brief 高连接节点保留全部边，且共用地面的两个独立物体不被合并。 */
void testHighDegreeAndBoundary()
{
    std::vector<std::unique_ptr<Particle>> particles;
    ContactTestScene scene;
    for (int i=0;i<49;++i) {
        particles.push_back(std::make_unique<Particle>(Eigen::Vector3d::Zero(),Eigen::Vector3d::Zero(),i==0 ? 8.0:1.0,i+1));
        scene.bodies.push_back(handle(*particles.back()));
    }
    std::vector<SolverContactConstraint> contacts;
    for (int i=1;i<49;++i) {
        const double angle=2*3.141592653589793*i/48;
        SolverContactConstraint c; c.bodyBIndex=i; c.contactNormal={std::cos(angle),std::sin(angle),0}; c.penetration=0.01;
        contacts.push_back(c);
    }
    SolverConfig config; config.positionIterations=1; config.positionCorrectionBeta=1; config.penetrationSlop=0;
    config.graphIterations=4096; config.positionTolerance=1e-9; SolverStats stats;
    ContactGraphProjector::project(scene.bodies,contacts,config,stats);
    require(stats.contactIslandCount==1 && stats.maxResidualPenetration<1e-7,"高连接图未整体恢复穿透");
    Eigen::Vector3d center=Eigen::Vector3d::Zero();
    for (auto& particle:particles) center+=particle->getMass()*particle->getPosition();
    require(center.norm()<1e-10,"高连接投影改变质量中心");
    particles[0]->setMass(0); scene.bodies[0]=handle(*particles[0]);
    stats={}; contacts.resize(2);
    ContactGraphProjector::project(scene.bodies,contacts,config,stats);
    require(stats.contactIslandCount==2,"静态地面错误合并独立动态岛");
}

/** @brief 检查惯性非对角项和空合力显式初始化，防止内存状态改变物理结果。 */
void testInitializedPhysicsState()
{
    ShapeBox box(100,100,100); ShapeCylinder cylinder(100,200);
    for (const auto inertia : {box.getInertiaTensor(10),cylinder.getInertiaTensor(10)}) {
        require(inertia.allFinite(),"惯性矩阵含非有限值");
        require((inertia-inertia.diagonal().asDiagonal().toDenseMatrix()).norm()==0,"轴对称标准形状惯性非对角项不为零");
    }
    Particle particle(Eigen::Vector3d::Zero(),Eigen::Vector3d::Zero(),1,1);
    require(particle.getJoinForce().norm()==0,"空合力累加器没有清零");
}

/** @brief 验证低速无回弹、恢复目标固定、摩擦圆锥和多点支撑合力矩。 */
void testVelocity()
{
    RigidBody ground(0.0,1), body(10.0,2);
    ground.setShape(std::make_shared<ShapeBox>(10000,10000,130));
    body.setShape(std::make_shared<ShapeBox>(100,100,100)); body.setPosition({300,400,115});
    ContactTestScene scene; scene.bodies={handle(ground),handle(body)};
    const auto points=buildContactManifold(ground,body,0.0,Eigen::Vector3d::Zero(),0.0,Eigen::Vector3d::Zero());
    std::vector<SolverContactConstraint> contacts;
    for (const auto& point:points) {
        SolverContactConstraint row;
        row.bodyBIndex=1; row.contactPosition=point.position; row.contactNormal=point.normal;
        row.restitution=1; row.frictionCoefficient=0.5; contacts.push_back(row);
    }
    SolverConfig config; config.velocityIterations=128;
    FrictionalContactResolverStrategy resolver; SolverStats stats;
    body.setSpeed({0,0,-0.09});
    resolver.resolveContacts(scene,contacts,0.0005,config,stats);
    require(body.getVelocity().norm()<1e-8 && body.getAngularVelocity().norm()<1e-8,"低速支撑仍有回弹或转动");
    body.setSpeed({0,0,-2}); body.setAngular(Eigen::Vector3d::Zero());
    resolver.resolveContacts(scene,contacts,0.0005,config,stats);
    require(std::abs(body.getVelocity().z()-2.0)<1e-8,"恢复目标没有保持");
    Particle particle(Eigen::Vector3d::Zero(), Eigen::Vector3d::Zero(), 1.0,4); particle.setSpeed({10,10,-1});
    scene.bodies={handle(ground),handle(particle)};
    SolverContactConstraint row; row.bodyBIndex=1; row.contactNormal=Eigen::Vector3d::UnitZ();
    row.restitution=0; row.frictionCoefficient=0.5;
    const Eigen::Vector3d old=particle.getVelocity();
    resolver.resolveContacts(scene,std::span<const SolverContactConstraint>(&row,1),0.001,config,stats);
    require((particle.getVelocity()-old).head<2>().norm()<=0.50000001,"摩擦超出圆锥");
}

/** @brief 杆约束与接触一起投影，固定端不会被错误移动。 */
void testJointProjection()
{
    Particle anchor(Eigen::Vector3d::Zero(), Eigen::Vector3d::Zero(), 0.0,1), body({2,0,0}, Eigen::Vector3d::Zero(), 1.0,2);
    std::vector<SolverBodyHandle> bodies{handle(anchor),handle(body)};
    ConnectingRod rod(1.0,&anchor,&body,3);
    std::vector<ConstraintsBase*> joints{&rod};
    SolverConfig config; config.positionCorrectionBeta=1; config.positionIterations=16;
    config.penetrationSlop=0; config.maxPositionCorrectionPerStep=2;
    SolverStats stats;
    ContactGraphProjector::project(bodies,{},config,stats,joints);
    require(std::abs(body.getPosition().x()-1)<1e-7,"杆未参与整体投影");
    require(anchor.getPosition().norm()==0,"固定端发生移动");
}

/** @brief 离线无需渲染回调也能休眠，撤走支撑与外力变化必须解冻。 */
void testContactSleep()
{
    AsyncLogger::instance().setOutputToFile(false);
    Particle ground(Eigen::Vector3d::Zero(), Eigen::Vector3d::Zero(), 0.0,1), body({0,0,1}, Eigen::Vector3d::Zero(), 1.0,2);
    body.upsertForce({{0,0,-10},42});
    ContactTestScene scene; scene.bodies={handle(ground),handle(body)};
    KineticSleepStrategy strategy;
    SleepStrategy& base=strategy;
    SolverContactConstraint contact; contact.bodyBIndex=1; contact.contactNormal=Eigen::Vector3d::UnitZ();
    const std::span<const SolverContactConstraint> contacts(&contact,1);
    for (int i=0;i<600;++i) base.evaluateSupported(scene,0.001,contacts);
    require(body.isAsleep(),"离线支撑休眠未生效");
    base.evaluateSupported(scene,0.001,{});
    require(!body.isAsleep(),"撤去支撑后未唤醒");
    for (int i=0;i<600;++i) base.evaluateSupported(scene,0.001,{});
    require(!body.isAsleep(),"重力下无支撑的低速实体被错误冻结");
    for (int i=0;i<600;++i) base.evaluateSupported(scene,0.001,contacts);
    require(body.isAsleep(),"重新支撑后无法休眠");
    body.upsertForce({{1,0,-10},42});
    base.evaluateSupported(scene,0.001,contacts);
    require(!body.isAsleep(),"外力变化后未唤醒");
}

/** @brief 无休眠条件下区分完全弹性弹跳和带阻力停稳，防止修复靠耗散掩盖问题。 */
void testGravitySimulation()
{
    for (int variant=0;variant<4;++variant) {
        const bool damped=variant%2!=0, cylinder=variant>=2;
        const double floorHeight=cylinder ? 165.0:115.0;
        const double initialEnergy=10.0*180.0*(500.0-floorHeight);
        RigidBody ground(0.0,1), body(10.0,2);
        ground.setShape(std::make_shared<ShapeBox>(10000,10000,130));
        body.setShape(std::make_shared<ShapeBox>(100,100,100));
        if (cylinder) body.setShape(std::make_shared<ShapeCylinder>(100,200));
        body.setPosition({300,400,500}); ground.setRestitution(1); body.setRestitution(1);
        GravityField gravity(Eigen::Vector3d(0,0,-180),Eigen::Vector3d::Zero(),100);
        AirResistance drag(101); DragConfig dragConfig; dragConfig.model=DragModel::Linear;
        dragConfig.linearCoeff=5; drag.addEntity(&body,dragConfig);
        ContactTestScene scene; scene.bodies={handle(ground),handle(body)}; scene.fields={&gravity};
        if (damped) scene.joints={&drag};
        SolverConfig config;
        Solver solver(config,std::make_unique<VelocityVerletIntegratorStrategy>(),
            std::make_unique<BruteForceBroadStrategy>(),std::make_unique<FrictionalContactResolverStrategy>());
        const int steps=damped ? 80000 : 20000;
        double minEnergy=1e30, maxEnergy=0, tailMin=1e30, tailMax=-1e30;
        for (int step=0;step<steps;++step) {
            try { static_cast<void>(solver.step(scene,0.0005)); }
            catch (...) {
                std::cerr<<"场景阻力="<<damped<<"，步="<<step<<"，位置="<<body.getPosition().transpose()
                    <<"，速度="<<body.getVelocity().transpose()<<"，角速度="<<body.getAngularVelocity().transpose()<<'\n';
                throw;
            }
            const double energy=body.getKineticEnergy()+body.getMass()*180*(body.getPosition().z()-floorHeight);
            minEnergy=std::min(minEnergy,energy); maxEnergy=std::max(maxEnergy,energy);
            if (step>steps-10000) { tailMin=std::min(tailMin,body.getPosition().z()); tailMax=std::max(tailMax,body.getPosition().z()); }
        }
        if (damped) {
            require(tailMax-tailMin<1e-6,"带阻力场景尾段仍有振幅");
            require(body.getVelocity().norm()<1e-7 && body.getAngularVelocity().norm()<1e-7,"无休眠条件下未停稳");
        } else {
            std::cout<<std::setprecision(17)<<"弹性场景圆柱="<<cylinder<<"，能量区间 "<<minEnergy<<" / "<<maxEnergy
                <<"，最大相对偏差="<<std::max(std::abs(minEnergy/initialEnergy-1),std::abs(maxEnergy/initialEnergy-1))<<'\n';
            require(minEnergy>0.98*initialEnergy && maxEnergy<1.02*initialEnergy,"完全弹性场景出现异常能量变化");
            require(tailMax-tailMin>100,"完全弹性无阻力场景被错误冻结或耗散");
        }
    }
}

/** @brief 执行全部独立回归并返回可用于 CTest 的退出码。 */
int main()
{
    try { testInitializedPhysicsState(); testManifolds(); testGraph(); testHighDegreeAndBoundary(); testVelocity(); testJointProjection(); testContactSleep(); testGravitySimulation(); std::cout<<"接触回归全部通过\n"; return 0; }
    catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
