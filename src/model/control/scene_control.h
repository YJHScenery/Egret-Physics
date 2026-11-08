/** @file scene_control.h @brief 与谓词实现解耦的场景控制输入、输出及程序接口。 */
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace egret::control
{
    using EntityId = std::uint64_t;
    using Vector = std::array<double,3>;

    /** @brief 按稳定标识排序的只读实体状态，不暴露引擎指针。 */
    struct BodyState
    {
        EntityId id{};
        Vector position{}, velocity{}, angularVelocity{};
        bool active{}, asleep{};
        double mass{};
        std::array<double,4> rotation{0,0,0,1};
        Vector material{0.5,0.5,0};
        bool rotatable{};
    };
    /** @brief 上一计算步中真实参与接触求解的实体对。 */
    struct ContactPair { EntityId first{}, second{}; };
    /** @brief 一次判断的输入快照，所有视图仅在 evaluate 调用期间有效。 */
    struct Frame
    {
        double time{}, step{};
        std::span<const BodyState> bodies;
        std::span<const ContactPair> contacts;
        std::uint64_t stepIndex{};
        std::span<const BodyState> templates;
        /** @brief 根据稳定标识查找实体，未找到返回空指针。 */
        const BodyState* find(EntityId id) const
        {
            const auto it=std::lower_bound(bodies.begin(),bodies.end(),id,
                /** @brief 按稳定标识比较快照条目。 */
                [](const BodyState& body,EntityId value){return body.id<value;});
            return it!=bodies.end() && it->id==id ? &*it : nullptr;
        }
        /** @brief 查询上一步是否发生过指定实体对的真实接触。 */
        bool touching(EntityId a,EntityId b) const
        {
            if (a>b) std::swap(a,b);
            const auto it=std::lower_bound(contacts.begin(),contacts.end(),ContactPair{a,b},
                /** @brief 按两个标识的字典序比较接触对。 */
                [](const ContactPair& x,const ContactPair& y){return x.first<y.first || (x.first==y.first && x.second<y.second);});
            return it!=contacts.end() && it->first==a && it->second==b;
        }
    };
    /** @brief 宿主支持的场景干预语义，与判断程序的实现方式无关。 */
    enum class CommandKind { SetPosition, SetVelocity, SetActive, SetForce, SetAngularVelocity, SetMass, Reserved, Spawn, Destroy, SetRotation, SetTorque, SetMaterial };
    /** @brief 延迟提交的干预值；启用状态使用 enabled，其余使用 value。 */
    struct Command
    {
        CommandKind kind{};
        EntityId target{};
        Vector value{};
        bool enabled{true};
        EntityId source{};
        double scalar{};
    };
    /** @brief 已提交动作的仿真时间与步序号，供结果诊断。 */
    struct AppliedCommand { Command command; double time{}; std::uint64_t step{}; };
    /** @brief 有容量限制的动作缓冲，后端不能直接修改物理世界。 */
    class CommandBuffer
    {
    public:
        /** @brief 在开始运行前预留动作容量。 */
        explicit CommandBuffer(std::size_t capacity=1024):m_capacity(capacity) {m_commands.reserve(capacity);}
        /** @brief 提交动作；达到预算时明确抛出异常，禁止静默截断。 */
        void append(const Command& command)
        {
            if (m_commands.size()==m_capacity) throw std::runtime_error("场景控制动作数量超出预算");
            m_commands.push_back(command);
        }
        /** @brief 读取已生成的动作。 */
        std::span<const Command> commands() const {return m_commands;}
        /** @brief 清空本次输出但保留内存。 */
        void clear() {m_commands.clear();}
    private:
        std::size_t m_capacity;
        std::vector<Command> m_commands;
    };
    /** @brief 控制程序抽象，谓词、C/Wasm 与 Lua 后端共享输入输出约定。
     * @details 这里只约定逻辑边界，不承诺 C++ 虚表、标准库或结构体具有稳定的机器码 ABI。
     * 各后端负责源码准备、资源限制及领域接口桥接；不得保留快照视图或直接访问求解器。
     */
    class Program
    {
    public:
        /** @brief 销毁程序及其执行状态。 */
        virtual ~Program()=default;
        /** @brief 在采样前判断是否到期，原生规则默认每步执行。 */
        virtual bool due(double) const {return true;}
        /** @brief 声明状态依赖；旧后端默认订阅全部实体。 */
        virtual bool observes(EntityId)const{return true;}
        /** @brief 判断是否需要累计接触；旧后端保持上一物理步语义。 */
        virtual bool wantsContacts(double)const{return true;}
        /** @brief 是否需要预建控制力场，旧后端默认可能输出持久力。 */
        virtual bool usesForces()const{return true;}
        /** @brief 在安全步边界读取同一快照并生成有限动作；失败由宿主中止本步。 */
        virtual void evaluate(const Frame& frame,CommandBuffer& output)=0;
        /** @brief 重置触发历史和用户状态，使重新运行具有确定语义。 */
        virtual void reset()=0;
    };
}
