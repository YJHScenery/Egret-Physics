/** @file test_lua_control.cpp @brief 验证三后端契约、Lua 隔离、资源限制与 SDK 生命周期。 */
#include "control_core/control_program.h"
#include "model/control/predicate_program.h"
#include <iostream>
#include <stdexcept>
using namespace egret::control;
/** @brief Release 构建中也执行验收断言。 */
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
/** @brief 生成独立 Lua 配置。 */
ControlProgramSpec luaSpec(const std::string& source){ControlProgramSpec s;s.backend="lua";s.source=source;s.bindings["body"]=1;return s;}
/** @brief 运行一次控制回调。 */
void run(Program& p,CommandBuffer& output,double time=0){BodyState body{1};body.mass=2;body.rotatable=true;p.evaluate({time,0.01,std::span(&body,1),{},static_cast<std::uint64_t>(time*100),std::span(&body,1)},output);}
/** @brief 验证无效脚本在准备或执行时明确失败。 */
void rejects(const std::string& source,bool execute=true,std::uint64_t fuel=10000){bool failed=false;try{auto s=luaSpec(source);s.fuel=fuel;auto p=prepareProgram(s,{});if(execute){CommandBuffer out;run(*p,out);}}catch(const std::exception&){failed=true;}check(failed,"无效 Lua 脚本没有被拒绝");}
/** @brief 验收入口。 */
int main(int argc,char** argv){try{
    auto specs=parseProgramSpecs(R"([{"source":"void on_step(void){}"},{"backend":"lua","source":"function on_step() end"}])");
    check(specs[0].backend=="c_wasm" && specs[1].backend=="lua","配置兼容性失败");
    auto s=luaSpec("function on_step() state_set_number('n',state_number('n')+1) entity_set_force(scene_entity('body'),vector3(state_number('n'),parameter_number('force'),control_delta())) end");s.parameters["force"]=3;
    auto a=prepareProgram(s,{}),b=prepareProgram(s,{});CommandBuffer out;
    run(*a,out);check(out.commands()[0].value==Vector{1,3,0},"Lua 首轮参数或状态错误");out.clear();
    run(*a,out,0.1);check(out.commands()[0].value==Vector{2,3,0.1},"跨步状态错误");out.clear();run(*b,out);check(out.commands()[0].value[0]==1,"程序状态未隔离");
    a->reset();out.clear();run(*a,out);check(out.commands()[0].value[0]==1,"重置未恢复状态");
    s.period=0.1;s.start=1;s.end=2;a=prepareProgram(s,{});check(!a->due(0)&&a->due(1)&&!a->due(2),"起止调度错误");out.clear();run(*a,out,1);check(!a->due(1.05)&&a->due(1.1),"控制周期错误");
    auto life=prepareProgram(luaSpec("function on_step() local e=scene_spawn(scene_template('body'),vector3(1,2,3)) entity_set_velocity(e,vector3(4,5,6)) state_set_entity('e',e) entity_set_mass(e,3) entity_set_rotation(e,rotation(0,0,0,1)) entity_set_material(e,material(0.4,0.2,0.1)) entity_set_torque(e,vector3(1,0,0)) entity_destroy(scene_entity('body')) end"),{},2);
    out.clear();run(*life,out);check(out.commands().size()==7 && out.commands()[0].kind==CommandKind::Spawn && out.commands()[1].target==out.commands()[0].target,"动态实体生命周期失败");
    auto containers=prepareProgram(luaSpec("local list=numbers_create() function on_step() numbers_push(list,4) local c=scene_entities() local e=collection_next(c) entity_set_force(e,vector3(numbers_pop(list),entity_mass(e),math_sqrt(9))) end"),{});
    out.clear();run(*containers,out);check(out.commands()[0].value==Vector{4,2,3},"集合或序列接口失败");
    rejects("function on_step( end",false);rejects("function on_step() while true do end end");rejects("while true do end function on_step() end",false);
    rejects("function on_step() local x={} while true do x={x,x,x,x,x,x,x,x,x,x} end end",true,100000000);
    rejects("function on_step() entity_set_force(1,vector3(0,0,0)) end");
    rejects("function on_step() entity_set_force(scene_entity('body'),vector3(0/0,0,0)) end");
    rejects("function on_step() for i=1,1100 do entity_set_force(scene_entity('body'),vector3(1,0,0)) end end",true,1000000);
    rejects("function on_step() io.open('x') end");rejects("function on_step() load('return 1') end");rejects("function on_step() pcall(function() while true do end end) end");
    rejects("function on_step() local function f() return 1+f() end f() end");
    bool cancel=false;CompilerOptions options;options.shouldCancel=/** @brief 在程序准备之后模拟取消。 */[&]{return cancel;};a=prepareProgram(luaSpec("function on_step() while true do end end"),options);cancel=true;
    bool canceled=false;try{out.clear();run(*a,out);}catch(const std::exception&){canceled=true;}check(canceled,"运行取消未生效");
    // 同一快照和动作契约下比较 C/Wasm、Lua 与谓词。
    if(argc==3){CompilerOptions cOptions;cOptions.clangPath=argv[1];cOptions.sdkDirectory=argv[2];cOptions.cacheDirectory="lua-test-cache";
        auto cSpec=luaSpec("void on_step(void){entity_set_force(scene_entity(\"body\"),vector3(3,0,0));}");cSpec.backend="c_wasm";
        auto c=prepareProgram(cSpec,cOptions);auto lua=prepareProgram(luaSpec("function on_step() entity_set_force(scene_entity('body'),vector3(3,0,0)) end"),{});
        PredicateProgram predicate({Rule{{Predicate{}},{{CommandKind::SetForce,1,{3,0,0}}},Trigger::WhileTrue}});
        CommandBuffer pOut;run(predicate,pOut);
        CommandBuffer cOut,lOut;run(*c,cOut);run(*lua,lOut);check(cOut.commands().size()==lOut.commands().size()&&cOut.commands()[0].value==lOut.commands()[0].value,"C/Lua 动作不等价");check(pOut.commands()[0].value==lOut.commands()[0].value,"谓词/Lua 动作不等价");
        std::vector<std::unique_ptr<Program>> programs;programs.push_back(std::move(c));programs.push_back(std::move(lua));CompositeProgram mixed(std::move(programs));out.clear();run(mixed,out);check(out.commands().size()==2,"混合执行失败");}
    std::cout<<"Lua 控制器验证通过\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
