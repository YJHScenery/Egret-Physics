/** @file test_scene_control.cpp @brief 验证控制后端的谓词、触发状态、动作预算与任意逻辑替换。 */
#include "control/predicate_program.h"
#include <iostream>
#include <limits>
#include <memory>

using namespace egret::control;

/** @brief 在 Release 模式下也保留测试断言。 */
void require(bool condition,const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

/** @brief 创建带指定节点及触发模式的测试规则。 */
Rule rule(std::vector<Predicate> nodes,Trigger trigger=Trigger::Once)
{
    return {std::move(nodes),{{CommandKind::SetVelocity,1,{5,0,0}}},trigger};
}

/** @brief 测试边沿、首次成立、持续成立及显式重置。 */
void testTriggers()
{
    Predicate time; time.kind=PredicateKind::Time; time.lower=1;
    for (auto trigger:{Trigger::Once,Trigger::Rising,Trigger::WhileTrue}) {
        PredicateProgram program({rule({time},trigger)});
        CommandBuffer out;
        Frame frame{0,0.01,{},{}};
        program.evaluate(frame,out); require(out.commands().empty(),"提前触发时间条件");
        frame.time=2; program.evaluate(frame,out); require(out.commands().size()==1,"时间条件未触发");
        out.clear(); program.evaluate(frame,out);
        require(out.commands().size()==(trigger==Trigger::WhileTrue ? 1:0),"重复触发语义错误");
        out.clear(); frame.time=0; program.evaluate(frame,out);
        frame.time=2; program.evaluate(frame,out);
        require(out.commands().size()==(trigger==Trigger::Once ? 0:1),"上升沿与一次触发未区分");
        out.clear(); program.reset(); program.evaluate(frame,out);
        require(out.commands().size()==1,"重置未清除触发历史");
    }
}

/** @brief 验证相对向量、距离、接触、休眠和布尔组合，缺失对象不能伪造状态。 */
void testPredicates()
{
    const std::vector<BodyState> bodies={{1,{3,4,0},{2,0,0},{0,0,2},true,false},
                                        {2,{0,0,0},{1,0,0},{0,0,1},true,true}};
    const std::vector<ContactPair> contacts={{1,2}};
    Frame frame{0,0.01,bodies,contacts};
    Predicate distance; distance.kind=PredicateKind::Distance;distance.first=1;distance.second=2;
    distance.comparison=Comparison::Near;distance.lower=5;
    Predicate relative;relative.kind=PredicateKind::RelativeVelocity;relative.first=1;relative.second=2;
    relative.component=Component::X;relative.comparison=Comparison::Near;relative.lower=1;
    Predicate both;both.kind=PredicateKind::All;both.left=0;both.right=1;
    PredicateProgram combined({rule({distance,relative,both})});
    CommandBuffer out;combined.evaluate(frame,out);require(out.commands().size()==1,"相对量组合判断错误");
    for (auto kind:{PredicateKind::RelativePosition,PredicateKind::Velocity,PredicateKind::AngularVelocity,
                    PredicateKind::RelativeAngularVelocity,PredicateKind::Position,PredicateKind::Active,
                    PredicateKind::Asleep,PredicateKind::Contact}) {
        Predicate p;p.kind=kind;p.first=1;p.second=2;p.comparison=Comparison::GreaterEqual;
        p.lower=0;p.expected=kind!=PredicateKind::Asleep;
        PredicateProgram program({rule({p})});out.clear();program.evaluate(frame,out);
        require(out.commands().size()==1,"运动量或状态谓词判断错误");
        p.first=99;PredicateProgram missing({rule({p})});out.clear();missing.evaluate(frame,out);
        require(out.commands().empty(),"缺失实体被判断为有效状态");
    }
    Predicate inside;inside.kind=PredicateKind::Time;inside.comparison=Comparison::Inside;inside.lower=1;inside.upper=2;
    PredicateProgram interval({rule({inside},Trigger::WhileTrue)});
    frame.time=1;out.clear();interval.evaluate(frame,out);require(out.commands().size()==1,"区间左端应包含");
    frame.time=2;out.clear();interval.evaluate(frame,out);require(out.commands().empty(),"区间右端应排除");
    Predicate negate;negate.kind=PredicateKind::Not;negate.left=0;
    Predicate any;any.kind=PredicateKind::Any;any.left=0;any.right=1;
    PredicateProgram boolean({rule({inside,negate,any})});out.clear();boolean.evaluate(frame,out);
    require(out.commands().size()==1,"或与非组合错误");
    require(frame.touching(2,1),"接触查询依赖实体输入顺序");
}

/** @brief 拒绝循环节点、无效枚举、非法区间和非有限输出，动作溢出明确报错。 */
void testValidation()
{
    for (int variant=0;variant<5;++variant) {
        Predicate p;
        if(variant==0){p.kind=PredicateKind::Not;p.left=0;}
        if(variant==1)p.component=static_cast<Component>(99);
        if(variant==2)p.lower=std::numeric_limits<double>::quiet_NaN();
        if(variant==3){p.comparison=Comparison::Inside;p.lower=2;p.upper=1;}
        Rule invalid=rule({p});
        if(variant==4)invalid.commands[0].value[0]=std::numeric_limits<double>::infinity();
        bool rejected=false;try{PredicateProgram program({invalid});}catch(const std::invalid_argument&){rejected=true;}
        require(rejected,"非法配置未在构造时拒绝");
    }
    PredicateProgram program({rule({Predicate{}},Trigger::WhileTrue)});CommandBuffer out(1);
    Frame frame{};program.evaluate(frame,out);
    bool rejected=false;try{program.evaluate(frame,out);}catch(const std::runtime_error&){rejected=true;}
    require(rejected && out.commands().size()==1,"动作预算溢出被静默忽略");
}

/** @brief 模拟未来任意编译程序适配器，自定义判断无需增加谓词枚举。 */
class ArbitraryProgram final : public Program
{
public:
    /** @brief 使用自定义数学函数生成控制动作。 */
    void evaluate(const Frame& frame,CommandBuffer& output) override
    {
        const auto* body=frame.find(1);
        if(body && body->position[0]*body->position[0]+std::sin(frame.time)>2)
            output.append({CommandKind::SetForce,1,{-2*body->position[0]-body->velocity[0],0,0}});
    }
    /** @brief 此程序没有跨步用户状态。 */
    void reset() override {}
};

/** @brief 验证调用方只依赖抽象程序接口，可以替换为非枚举判断。 */
void testBackendReplacement()
{
    const std::vector<BodyState> bodies={{1,{3,0,0},{2,0,0},{},true,false}};
    Frame frame{0,0.01,bodies,{}};CommandBuffer out;
    std::unique_ptr<Program> program=std::make_unique<ArbitraryProgram>();
    program->evaluate(frame,out);
    require(out.commands().size()==1 && out.commands()[0].value[0]==-8,"抽象接口不能承载自定义函数逻辑");
}

/** @brief 运行所有控制层回归并输出可用于 CTest 的退出码。 */
int main()
{
    try {testTriggers();testPredicates();testValidation();testBackendReplacement();std::cout<<"场景控制回归通过\n";return 0;}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
