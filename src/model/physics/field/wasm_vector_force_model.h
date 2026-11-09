/** @file wasm_vector_force_model.h @brief 将受限 C/Wasm 程序适配为可采样的矢量力场。 */
#pragma once
#include "vector_force_model.h"
#include "control_core/control_program.h"
#include <mutex>

namespace egret {
/** @brief 从受限 C 源码构造场；场程序为无状态函数，编译失败抛出诊断。 */
inline VectorForceModel makeWasmVectorForceModel(const std::string& source,
                                                  const control::CompilerOptions& options,
                                                  std::uint64_t fuel = 1000000)
{
    struct State {
        /** @brief 持有隔离的 Wasm 实例与可复用输出。 */
        explicit State(std::unique_ptr<control::Program> p) : program(std::move(p)), output(1) {}
        std::mutex mutex;
        std::unique_ptr<control::Program> program;
        control::CommandBuffer output;
    };
    auto state = std::make_shared<State>(control::prepareVectorFieldProgram(source, options, fuel));
    return VectorForceModel([](const Eigen::Vector3d&) { return true; },
        [state](double time, const Eigen::Vector3d& position) -> Eigen::Vector3d {
            if (time < 0 || time >= 1e100) throw std::invalid_argument("Wasm 场采样时间超出范围");
            std::scoped_lock lock(state->mutex);
            control::BodyState body; body.id=1; body.active=true; body.position={position.x(),position.y(),position.z()};
            state->output.clear();
            state->program->evaluateSample({time,0,std::span(&body,1),{}},state->output);
            const auto commands=state->output.commands();
            if (commands.size()!=1 || commands[0].kind!=control::CommandKind::SetForce || commands[0].target!=1)
                throw std::runtime_error("Wasm 场输出不符合约定");
            const auto& v=commands[0].value;
            return {v[0],v[1],v[2]};
        });
}
}
