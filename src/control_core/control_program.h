/** @file control_program.h @brief 无 Qt 的控制源码配置、编译诊断与 Wasmtime 执行适配。 */
#pragma once
#include "model/control/scene_control.h"
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace egret::control
{
/** @brief 可持久化的单个控制程序，配置不包含编译产物或宿主指针。 */
struct ControlProgramSpec
{
    std::string name{"控制程序"};
    std::string source{"/** @brief 场景控制入口。 */\nvoid on_step(void)\n{\n}\n"};
    bool enabled{true};
    double period{};
    double start{};
    double end{1e100};
    std::uint64_t fuel{1000000};
    std::map<std::string, EntityId> bindings;
    std::map<std::string, double> parameters;
    /** 全场景初始身份，由宿主目录提供，避免动态实体与场、约束等对象冲突。 */
    std::vector<EntityId> reservedIdentities;
};
/** @brief 单个程序的准备统计，不包括物理推进。 */
struct PreparationReport
{
    std::string name, digest;
    double validationSeconds{}, cCompileSeconds{}, machineCompileSeconds{}, instanceSeconds{};
    bool cacheHit{};
};
/** @brief 核心编译环境，可用于无 Qt 命令行宿主。 */
struct CompilerOptions
{
    std::filesystem::path clangPath, sdkDirectory, cacheDirectory;
    std::function<bool()> shouldCancel;
    std::function<void(const PreparationReport &)> onPrepared;
};
/** @brief 创建已完成语言校验和编译的独立程序；失败抛出源码诊断。 */
std::unique_ptr<Program> prepareProgram(const ControlProgramSpec &spec, const CompilerOptions &options,
                                        unsigned identityNamespace = 1);
/** @brief 返回部署或环境变量指定的编译环境。 */
CompilerOptions defaultCompilerOptions();
/** @brief 在无 Qt 核心解析版本化配置，旧场景补默认空入口。 */
std::vector<ControlProgramSpec> parseProgramSpecs(const std::string &json);
/** @brief 将原生规则与多个源码后端按顺序组合，同轮共享快照与动作总预算。 */
class CompositeProgram final : public Program
{
  public:
    /** @brief 安装已准备的独立后端。 */
    explicit CompositeProgram(std::vector<std::unique_ptr<Program>> programs)
        : m_programs(std::move(programs)), m_contacts(m_programs.size())
    {
    }
    /** @brief 至少一个后端到期时才需要采样。 */
    bool due(double time) const override;
    /** @brief 合并所有后端的实体订阅。 */
    bool observes(EntityId id) const override;
    /** @brief 至少一个仍有效的后端订阅接触时才采集。 */
    bool wantsContacts(double time) const override;
    /** @brief 任意后端可能施力时才预建控制力场。 */
    bool usesForces() const override;
    /** @brief 依次生成动作；任一失败由世界丢弃整批输出。 */
    void evaluate(const Frame &frame, CommandBuffer &output) override;
    /** @brief 重置所有独立实例。 */
    void reset() override;

  private:
    std::vector<std::unique_ptr<Program>> m_programs;
    std::vector<std::vector<ContactPair>> m_contacts;
};
} // namespace egret::control
