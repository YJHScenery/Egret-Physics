/** @file test_programmable_control.cpp @brief 无 Qt 宿主验证真实 C 编译、状态隔离、语义限制和 fuel 终止。 */
#include "control_core/control_program.h"
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <new>
#include <stdexcept>
static thread_local bool countAllocations = false;
static thread_local std::size_t allocationCount = 0;
/** @brief 仅计数测试线程的 C++ 桥接分配，不冒充整个 Rust 运行时或求解器分配计数。 */
void *operator new(std::size_t bytes)
{
    if (countAllocations)
        ++allocationCount;
    if (void *value = std::malloc(bytes ? bytes : 1))
        return value;
    throw std::bad_alloc();
}
/** @brief 释放测试计数分配。 */
void operator delete(void *value) noexcept
{
    std::free(value);
}
/** @brief 支持 MSVC 有尺寸释放。 */
void operator delete(void *value, std::size_t) noexcept
{
    std::free(value);
}
using namespace egret::control;
/** @brief 无论构建配置均执行断言。 */
void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
/** @brief 验证源码在准备或执行阶段被明确拒绝。 */
void rejects(const std::string &source, const CompilerOptions &options, bool execute = false,
             std::uint64_t fuel = 1000000, const std::string &reason = {})
{
    ControlProgramSpec spec;
    spec.source = source;
    spec.bindings["body"] = 1;
    spec.fuel=fuel;
    bool rejected = false;
    try
    {
        auto p = prepareProgram(spec, options);
        if (execute)
        {
            CommandBuffer output;
            BodyState body{1};
            p->evaluate({0, 0.01, std::span(&body, 1), {}}, output);
        }
    }
    catch (const std::exception &error)
    {
        rejected = true;
        if(!reason.empty())require(std::string(error.what()).find(reason)!=std::string::npos,"程序被错误的预算或原因中断");
        std::cout << error.what() << '\n';
    }
    require(rejected, "无效程序未被拒绝");
}
/** @brief 在未链接 Qt 的可执行文件中验证完整编译执行链路。 */
int main(int argc, char **argv)
{
    try
    {
        require(argc == 3, "需要 Clang 与 SDK 路径");
        CompilerOptions options{argv[1], argv[2], std::filesystem::temp_directory_path() / "egret-control-tests", {}};
        ControlProgramSpec spec;
        spec.bindings["body"] = 1;
        spec.period = 0.1;
        spec.source = R"(
/** @brief 测试持久状态与力动作。 */
void on_step(void) { Entity body=scene_entity("body"); state_set_number("count",state_number("count")+1); entity_set_force(body,vector3(state_number("count"),control_delta(),entity_position(body).x)); }
)";
        auto p = prepareProgram(spec, options);
        auto other = prepareProgram(spec, options, 2);
        BodyState body{1, {3, 0, 0}};
        CommandBuffer output;
        p->evaluate({0, 0.01, std::span(&body, 1), {}, 0}, output);
        require(output.commands().size() == 1 && output.commands()[0].value[0] == 1, "首次执行失败");
        require(!p->due(0.05) && p->due(0.1), "调度失败");
        output.clear();
        p->evaluate({0.1, 0.01, std::span(&body, 1), {}, 10}, output);
        require(output.commands()[0].value[0] == 2 && std::abs(output.commands()[0].value[1] - 0.1) < 1e-12,
                "跨步状态或控制步长失败");
        output.clear();
        other->evaluate({0, 0.01, std::span(&body, 1), {}}, output);
        require(output.commands()[0].value[0] == 1, "世界状态发生串扰");
        p->reset();
        output.clear();
        p->evaluate({0, 0.01, std::span(&body, 1), {}}, output);
        require(output.commands()[0].value[0] == 1, "重置未恢复状态");
        rejects("void on_step(void){double a[2];a[0]=1;}", options);
        rejects("void on_step(void){Entity e={1};}", options);
        rejects("void on_step(void){Entity e=scene_entity(\"body\");e._value=3;}", options);
        rejects("void again(void){again();} void on_step(void){again();}", options);
        rejects("void on_step(void){scene_entity(\"missing\");}", options);
        rejects("void on_step(void){while(1){state_set_number(\"x\",state_number(\"x\")+1);}}", options, true);
        rejects("void on_step(void){Entity e=scene_entity(\"body\");for(int "
                "i=0;i<1100;i++)entity_set_force(e,vector3(1,0,0));}",
                options, true,100000000,"unreachable");
        rejects("/* 前置注释 */ #include \"../control_abi.h\"\nvoid on_step(void){}", options);
        spec.source = "void on_step(void){Entity "
                      "e=scene_spawn(scene_template(\"body\"),vector3(1,2,3));entity_set_velocity(e,vector3(4,5,6));"
                      "state_set_entity(\"new\",e);}";
        p = prepareProgram(spec, options);
        output.clear();
        p->evaluate({0, 0.01, std::span(&body, 1), {}}, output);
        require(output.commands().size() == 2 && output.commands()[0].kind == CommandKind::Spawn &&
                    output.commands()[1].target == output.commands()[0].target,
                "同批创建配置失败");
        spec.parameters["gain"] = 1e-12;
        spec.source = R"(
/** @brief 测试数值结构、参数与无地址集合。 */
struct Feedback { double gain; };
void on_step(void) {
    struct Feedback feedback; feedback.gain=parameter_number("gain");
    NumberList values=numbers_create();numbers_push(values,feedback.gain);
    EntityCollection entities=scene_entities();Entity e=collection_next(entities);
    while(entity_exists(e)){entity_set_force(e,vector3(numbers_count(values),numbers_pop(values),0));e=collection_next(entities);}
    numbers_destroy(values);
})";
        p = prepareProgram(spec, options);
        output.clear();
        p->evaluate({0, 0.01, std::span(&body, 1), {}}, output);
        require(output.commands().size() == 1 && output.commands()[0].value[1] == 1e-12,
                "类型化容器、集合或参数精度失败");
        spec.period = 0;
        spec.source =
            "void on_step(void){EntityCollection c=scene_entities();Entity "
            "e=collection_next(c);if(entity_exists(e))entity_set_force(e,vector3(parameter_number(\"gain\"),0,0));}";
        p = prepareProgram(spec, options);
        for (std::size_t size : {10u, 100u, 1000u})
        {
            p->reset();
            std::vector<BodyState> states(size);
            for (std::size_t i = 0; i < size; ++i)
                states[i].id = i + 1;
            output.clear();
            p->evaluate({1, 0.001, states, {}}, output);
            allocationCount = 0;
            countAllocations = true;
            auto started = std::chrono::steady_clock::now();
            for (unsigned i = 0; i < 200; ++i)
            {
                output.clear();
                p->evaluate({2 + i * 0.001, 0.001, states, {}, i}, output);
            }
            auto elapsed = std::chrono::steady_clock::now() - started;
            countAllocations = false;
            require(allocationCount == 0, "稳态 C++ 控制桥接出现重复分配");
            std::cout << size << " 个实体，每次输入复制、执行与解码平均 "
                      << std::chrono::duration<double, std::micro>(elapsed).count() / 200 << " 微秒，C++ 桥接分配 "
                      << allocationCount << '\n';
        }
        for (unsigned branches : {10u, 100u, 1000u})
        {
            spec.source = "void on_step(void){Entity body=scene_entity(\"body\");";
            for (unsigned i = 0; i < branches; ++i)
                spec.source += "if(entity_position(body).x>" + std::to_string(1000 + i) +
                               ")entity_set_force(body,vector3(1,0,0));";
            spec.source += "}";
            spec.fuel = 100000000;
            p = prepareProgram(spec, options);
            output.clear();
            p->evaluate({0, 0.001, std::span(&body, 1), {}}, output);
            auto started = std::chrono::steady_clock::now();
            for (unsigned i = 0; i < 50; ++i)
            {
                output.clear();
                p->evaluate({i * 0.001, 0.001, std::span(&body, 1), {}, i}, output);
            }
            std::cout << branches << " 个 C 条件分支平均 "
                      << std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started).count() /
                             50
                      << " 微秒\n";
        }
        spec.source="void on_step(void){state_set_number(\"x\",1);}";
        auto parallelOptions=options;parallelOptions.cacheDirectory/=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        auto first=std::async(std::launch::async,/** @brief 模拟编辑器草稿与正式求解并行准备同一源码。 */[&]{return prepareProgram(spec,parallelOptions);});
        auto second=std::async(std::launch::async,/** @brief 第二个任务拥有独立临时文件和执行状态。 */[&]{return prepareProgram(spec,parallelOptions);});
        auto firstProgram=first.get(),secondProgram=second.get();
        PreparationReport cachedReport;parallelOptions.onPrepared=[&](const PreparationReport& report){cachedReport=report;};
        auto cachedProgram=prepareProgram(spec,parallelOptions);require(cachedReport.cacheHit,"并行准备后缓存无法完整读取");
        auto cancelledOptions=options;cancelledOptions.shouldCancel=[](){return true;};bool cancelled=false;
        try{prepareProgram(spec,cancelledOptions);}catch(const std::exception&){cancelled=true;}require(cancelled,"控制准备未响应取消");
        std::cout << "无 Qt 控制核心验证通过\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
