/** @file lua_program.cpp @brief 受限 Lua 控制器，共享可信 SDK、快照与延迟动作提交。 */
#include "control_program.h"
#include "control_abi.h"
#include <lua.h>
#include <lauxlib.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <tuple>
#include <type_traits>
#include <utility>
namespace egret::control {
namespace {
namespace sdk {
#include "sdk/egret_control.h"
/** @brief 从同一可信 SDK 生成的实例私有运行库。 */
struct Runtime {
#include "native_runtime.inc"
};
}
/** @brief 每个 Lua 实例的内存计量。 */
struct MemoryBudget { std::size_t used{}; static constexpr std::size_t limit=8*1024*1024; };
/** @brief 所有 Lua 堆分配均通过此入口，超过容量时拒绝分配。 */
void* allocate(void* context,void* pointer,std::size_t oldSize,std::size_t newSize) {
    auto& budget=*static_cast<MemoryBudget*>(context);
    if(!pointer) oldSize=0;
    if(!newSize) {std::free(pointer);budget.used-=oldSize;return nullptr;}
    if(newSize>MemoryBudget::limit || budget.used-oldSize>MemoryBudget::limit-newSize) return nullptr;
    auto result=std::realloc(pointer,newSize);
    if(result) budget.used=budget.used-oldSize+newSize;
    return result;
}
/** @brief 为不同领域句柄提供独立且不可伪造的类型标识。 */
template<class T> const char* handleName() {
    if constexpr(std::is_same_v<T,sdk::Entity>) return "egret.Entity";
    if constexpr(std::is_same_v<T,sdk::EntityTemplate>) return "egret.EntityTemplate";
    if constexpr(std::is_same_v<T,sdk::EntityCollection>) return "egret.EntityCollection";
    return "egret.NumberList";
}
/** @brief 读取表中的有限数值字段，禁止触发用户元方法。 */
double field(lua_State* state,int index,const char* name) {
    luaL_checktype(state,index,LUA_TTABLE);lua_pushstring(state,name);lua_rawget(state,index);
    double value=luaL_checknumber(state,-1);lua_pop(state,1);
    if(!std::isfinite(value)) luaL_error(state,"数值必须有限");return value;
}
/** @brief 将 Lua 参数转换成共享 SDK 值。 */
template<class T> T read(lua_State* state,int index) {
    if constexpr(std::is_same_v<T,const char*>) {
        std::size_t length{};auto value=luaL_checklstring(state,index,&length);
        if(length>63 || std::memchr(value,0,length)) luaL_error(state,"名称必须为至多 63 字节的字符串");return value;
    } else if constexpr(std::is_same_v<T,bool>) {luaL_checktype(state,index,LUA_TBOOLEAN);return lua_toboolean(state,index)!=0;}
    else if constexpr(std::is_arithmetic_v<T>) {double value=luaL_checknumber(state,index);if(!std::isfinite(value))luaL_error(state,"数值必须有限");return static_cast<T>(value);}
    else if constexpr(std::is_same_v<T,sdk::Vector3>) return {field(state,index,"x"),field(state,index,"y"),field(state,index,"z")};
    else if constexpr(std::is_same_v<T,sdk::Rotation>) return {field(state,index,"x"),field(state,index,"y"),field(state,index,"z"),field(state,index,"w")};
    else if constexpr(std::is_same_v<T,sdk::Material>) return {field(state,index,"staticFriction"),field(state,index,"dynamicFriction"),field(state,index,"restitution")};
    else return *static_cast<T*>(luaL_checkudata(state,index,handleName<T>()));
}
/** @brief 写入一个数值字段。 */
void put(lua_State* state,const char* name,double value) {lua_pushnumber(state,value);lua_setfield(state,-2,name);}
/** @brief 将共享 SDK 值转换为 Lua 值或有类型的只读句柄。 */
template<class T> int push(lua_State* state,T value) {
    if constexpr(std::is_same_v<T,bool>) lua_pushboolean(state,value);
    else if constexpr(std::is_integral_v<T>) lua_pushinteger(state,static_cast<lua_Integer>(value));
    else if constexpr(std::is_floating_point_v<T>) {if(!std::isfinite(value))return luaL_error(state,"结果数值必须有限");lua_pushnumber(state,value);}
    else if constexpr(std::is_same_v<T,sdk::Vector3> || std::is_same_v<T,sdk::Rotation>) {
        lua_newtable(state);put(state,"x",value.x);put(state,"y",value.y);put(state,"z",value.z);
        if constexpr(std::is_same_v<T,sdk::Rotation>)put(state,"w",value.w);
    } else if constexpr(std::is_same_v<T,sdk::Material>) {
        lua_newtable(state);put(state,"staticFriction",value.staticFriction);put(state,"dynamicFriction",value.dynamicFriction);put(state,"restitution",value.restitution);
    } else {auto slot=static_cast<T*>(lua_newuserdatauv(state,sizeof(T),0));*slot=value;luaL_setmetatable(state,handleName<T>());}
    return 1;
}
/** @brief 解释型后端独立拥有 Lua 状态及共享 SDK 的固定容量状态。 */
class LuaProgram final:public Program {
public:
    /** @brief 保存配置并创建受限运行时。 */
    LuaProgram(ControlProgramSpec spec,CompilerOptions options,unsigned identity):spec_(std::move(spec)),options_(std::move(options)),identity_(identity) {reset();}
    /** @brief 释放解释器。 */
    ~LuaProgram() override {if(state_)lua_close(state_);}
    /** @brief 使用与 Wasm 后端一致的步边界调度。 */
    bool due(double time)const override {return time+1e-12>=next_ && time>=spec_.start && time<spec_.end;}
    /** @brief Lua 支持动态枚举，订阅全体实体。 */
    bool observes(EntityId)const override{return true;}
    /** @brief 调度区间内累计接触。 */
    bool wantsContacts(double time)const override{return time<spec_.end;}
    /** @brief 重新创建实例，恢复全局变量、具名状态和容器。 */
    void reset() override {
        if(state_)lua_close(state_);state_=nullptr;runtime_=std::make_unique<sdk::Runtime>();memory_={};
        next_=spec_.start;last_=0;first_=true;failed_=false;prefix_=0;fuel_=spec_.fuel;
        state_=lua_newstate(allocate,&memory_);if(!state_)throw std::runtime_error("无法创建 Lua 解释器");
        *static_cast<LuaProgram**>(lua_getextraspace(state_))=this;
        lua_sethook(state_,hook,LUA_MASKCOUNT,100);
        lua_pushcfunction(state_,initialize);
        if(lua_pcall(state_,0,0,0)!=LUA_OK) {auto message=error();lua_close(state_);state_=nullptr;throw std::runtime_error(message);}
    }
    /** @brief 填充权威快照，执行回调并输出经过领域运行库生成的动作。 */
    void evaluate(const Frame& frame,CommandBuffer& output) override {
        if(failed_)throw std::runtime_error("Lua 程序已失败，必须重置后重新运行");
        if(!due(frame.time))return;
        try {
            if(frame.bodies.size()>EGRET_BODY_CAPACITY || frame.templates.size()>EGRET_BODY_CAPACITY || frame.contacts.size()>EGRET_CONTACT_CAPACITY)throw std::runtime_error("控制输入超过容量");
            if(!prefix_) {
                prefix_=0x8000000000000000ULL | (static_cast<EntityId>(identity_)<<32);
                /** @brief 检查候选身份区间是否与初始场景冲突。 */
                auto collision=[&] {for(auto id:spec_.reservedIdentities)if((id&0xffffffff00000000ULL)==prefix_)return true;
                    for(auto b:frame.templates)if((b.id&0xffffffff00000000ULL)==prefix_)return true;
                    for(auto b:frame.bodies)if((b.id&0xffffffff00000000ULL)==prefix_)return true;return false;};
                while(collision()){prefix_+=0x1000000000000ULL;if(!(prefix_>>63))throw std::runtime_error("动态身份区间耗尽");}
            }
            auto& input=runtime_->exchange;std::memset(&input,0,sizeof(input));
            input.version=EGRET_ABI_VERSION;input.bytes=sizeof(input);input.time=frame.time;input.step=frame.step;
            input.delta=first_?0:frame.time-last_;input.stepIndex=frame.stepIndex;input.identityPrefix=prefix_;
            for(const auto& [name,id]:spec_.bindings){auto& b=input.bindings[input.bindingCount++];b.id=id;std::memcpy(b.name,name.c_str(),name.size()+1);}
            for(const auto& b:frame.bodies)input.bodies[input.bodyCount++]=pack(b);
            for(const auto& b:frame.templates)input.templates[input.templateCount++]=pack(b);
            for(const auto& p:frame.contacts)input.contacts[input.contactCount++]={p.first,p.second};
            runtime_->iteratorCount=0;if(++runtime_->iteratorGeneration>=0x800000000000000ULL)throw std::runtime_error("迭代器代次耗尽");
            fuel_=spec_.fuel;
            lua_pushcfunction(state_,step);
            if(lua_pcall(state_,0,0,0)!=LUA_OK)throw std::runtime_error(error());
            for(unsigned i=0;i<input.commandCount;++i){const auto& c=input.commands[i];
                output.append({static_cast<CommandKind>(c.kind),c.target,{c.value.x,c.value.y,c.value.z},c.enabled!=0,c.source,c.scalar});}
            last_=frame.time;first_=false;
            next_=spec_.period>0?spec_.start+(std::floor((frame.time-spec_.start)/spec_.period+1e-9)+1)*spec_.period:frame.time;
        }catch(const std::exception& e){failed_=true;throw std::runtime_error(spec_.name+" [Lua]，步 "+std::to_string(frame.stepIndex)+"，仿真时间 "+std::to_string(frame.time)+" 秒："+e.what());}
    }
private:
    /** @brief 将世界快照转为两后端共享的 SDK 数据布局。 */
    static EgretBody pack(const BodyState& b) {return {b.id,{b.position[0],b.position[1],b.position[2]},{b.velocity[0],b.velocity[1],b.velocity[2]},
        {b.angularVelocity[0],b.angularVelocity[1],b.angularVelocity[2]},b.mass,b.active,b.rotatable,{b.rotation[0],b.rotation[1],b.rotation[2],b.rotation[3]},
        {b.material[0],b.material[1],b.material[2]}};}
    /** @brief 从解释器额外空间读取私有宿主。 */
    static LuaProgram& self(lua_State* state){return **static_cast<LuaProgram**>(lua_getextraspace(state));}
    /** @brief 每一百条解释器指令检查预算和取消。 */
    static void hook(lua_State* state,lua_Debug*) {auto& p=self(state);if(p.options_.shouldCancel && p.options_.shouldCancel())luaL_error(state,"控制执行已取消");
        if(p.fuel_<100)luaL_error(state,"Lua 指令预算耗尽");p.fuel_-=100;}
    /** @brief 在保护调用中查找并调用用户入口。 */
    static int step(lua_State* state){lua_getglobal(state,"on_step");if(!lua_isfunction(state,-1))return luaL_error(state,"必须定义 function on_step()");lua_call(state,0,0);return 0;}
    /** @brief 提取并弹出脚本诊断。 */
    std::string error(){const char* message=lua_tostring(state_,-1);std::string result=message?message:"Lua 执行失败";lua_pop(state_,1);return result;}
    /** @brief 使用参数包按确定顺序转换并调用 SDK 成员。 */
    template<auto Function,class R,class... A,std::size_t... I>
    static int invoke(lua_State* state,R(sdk::Runtime::*)(A...),std::index_sequence<I...>){
        std::tuple<A...> args{read<A>(state,static_cast<int>(I)+1)...};
        if constexpr(std::is_void_v<R>){std::apply(/** @brief 转发已转换的领域参数。 */[&](auto... values){(self(state).runtime_.get()->*Function)(values...);},args);return 0;}
        else return push(state,std::apply(/** @brief 转发已转换的领域参数。 */[&](auto... values){return (self(state).runtime_.get()->*Function)(values...);},args));
    }
    /** @brief 从成员签名生成绑定，统一转换宿主异常。 */
    template<auto Function,class R,class... A>
    static int call(lua_State* state,R(sdk::Runtime::*signature)(A...)){
        return invoke<Function>(state,signature,std::index_sequence_for<A...>{});
    }
    /** @brief 绑定入口不允许 C++ 异常越过 Lua 保护边界。 */
    template<auto Function> static int binding(lua_State* state){
        try{return call<Function>(state,Function);}catch(const std::exception& e){lua_pushstring(state,e.what());}
        return lua_error(state);
    }
    /** @brief 读取公共数值参数，保持与 C 常量降级后的值一致。 */
    static int parameter(lua_State* state){auto key=read<const char*>(state,1);const auto& values=self(state).spec_.parameters;
        auto it=values.find(key);if(it==values.end())return luaL_error(state,"未绑定数值参数：%s",key);return push(state,it->second);}
    /** @brief 在保护调用中安装白名单和源码，初始化也受相同预算限制。 */
    static int initialize(lua_State* state){auto& p=self(state);
        // 不打开标准库；仅保留有限数学和领域 API，无法加载代码或捕获预算错误。
        for(auto name:{"egret.Entity","egret.EntityTemplate","egret.EntityCollection","egret.NumberList"}){
            luaL_newmetatable(state,name);lua_pushliteral(state,"受保护的领域句柄");lua_setfield(state,-2,"__metatable");lua_pop(state,1);}
#define EGRET_BIND(name) lua_pushcfunction(state,binding<&sdk::Runtime::name>);lua_setglobal(state,#name)
        // 共享 SDK 绑定在下方生成，参数由宿主提供。
        EGRET_BIND(vector3);EGRET_BIND(rotation);EGRET_BIND(material);
        EGRET_BIND(scene_entity);EGRET_BIND(scene_template);EGRET_BIND(scene_entities);EGRET_BIND(collection_next);
        EGRET_BIND(entity_exists);EGRET_BIND(entity_position);EGRET_BIND(entity_velocity);EGRET_BIND(entity_angular_velocity);
        EGRET_BIND(entity_mass);EGRET_BIND(entity_rotatable);EGRET_BIND(entity_rotation);EGRET_BIND(entity_material);EGRET_BIND(entity_active);EGRET_BIND(entities_touching);
        EGRET_BIND(entity_set_position);EGRET_BIND(entity_set_velocity);EGRET_BIND(entity_set_force);EGRET_BIND(entity_set_active);
        EGRET_BIND(entity_set_angular_velocity);EGRET_BIND(entity_set_mass);EGRET_BIND(entity_set_rotation);EGRET_BIND(entity_set_torque);EGRET_BIND(entity_set_material);
        EGRET_BIND(scene_spawn);EGRET_BIND(entity_destroy);EGRET_BIND(simulation_time);EGRET_BIND(simulation_step);EGRET_BIND(control_delta);EGRET_BIND(simulation_step_index);
        EGRET_BIND(state_number);EGRET_BIND(state_set_number);EGRET_BIND(state_bool);EGRET_BIND(state_set_bool);EGRET_BIND(state_entity);EGRET_BIND(state_set_entity);
        EGRET_BIND(numbers_create);EGRET_BIND(numbers_destroy);EGRET_BIND(numbers_push);EGRET_BIND(numbers_pop);EGRET_BIND(numbers_count);EGRET_BIND(math_sqrt);EGRET_BIND(math_abs);
#undef EGRET_BIND
        lua_pushcfunction(state,parameter);lua_setglobal(state,"parameter_number");
        if(luaL_loadbufferx(state,p.spec_.source.data(),p.spec_.source.size(),"control.lua","t")!=LUA_OK)return lua_error(state);
        lua_call(state,0,0);
        lua_getglobal(state,"on_step");if(!lua_isfunction(state,-1))return luaL_error(state,"必须定义 function on_step()");lua_pop(state,1);return 0;
    }
    ControlProgramSpec spec_;CompilerOptions options_;unsigned identity_;
    MemoryBudget memory_;lua_State* state_{};std::unique_ptr<sdk::Runtime> runtime_;
    double next_{},last_{};bool first_{true},failed_{};EntityId prefix_{};std::uint64_t fuel_{};
};
}
/** @brief 通过统一工厂创建现成 Lua 解释器后端。 */
std::unique_ptr<Program> prepareLuaProgram(const ControlProgramSpec& spec,const CompilerOptions& options,unsigned identity){
    return std::make_unique<LuaProgram>(spec,options,identity);
}
}
