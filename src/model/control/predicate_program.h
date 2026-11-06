/** @file predicate_program.h @brief 使用已校验的枚举节点顺序求值的场景控制后端。 */
#pragma once
#include "scene_control.h"

namespace egret::control
{
    /** @brief 数值、状态及布尔组合谓词；节点只能引用位于自身之前的节点。 */
    enum class PredicateKind { Always, Time, Position, Velocity, AngularVelocity,
        RelativePosition, RelativeVelocity, RelativeAngularVelocity, Distance,
        Active, Asleep, Contact, All, Any, Not };
    /** @brief 区间采用左闭右开，近似相等使用显式绝对容差。 */
    enum class Comparison { Less, LessEqual, Greater, GreaterEqual, Near, Inside, Outside };
    /** @brief 向量选择坐标分量或模长。 */
    enum class Component { X, Y, Z, Magnitude };
    /** @brief 单个条件节点，缺失实体或非活动实体的运动条件统一为假。 */
    struct Predicate
    {
        PredicateKind kind{PredicateKind::Always};
        EntityId first{}, second{};
        Component component{Component::Magnitude};
        Comparison comparison{Comparison::Greater};
        double lower{}, upper{}, tolerance{1e-6};
        std::size_t left{}, right{};
        bool expected{true};
    };
    /** @brief 首次成立、每次上升沿及条件保持期间三种触发模式。 */
    enum class Trigger { Once, Rising, WhileTrue };
    /** @brief 一条规则的末节点为根，规则之间按输入顺序生成动作。 */
    struct Rule
    {
        std::vector<Predicate> predicates;
        std::vector<Command> commands;
        Trigger trigger{Trigger::Once};
    };
    /** @brief 原生枚举判断实现；构造时校验，逐步求值不解析文本或分配节点内存。 */
    class PredicateProgram final : public Program
    {
    public:
        /** @brief 校验所有节点并预分配求值状态，拒绝循环引用及非有限阈值。 */
        explicit PredicateProgram(std::vector<Rule> rules):m_rules(std::move(rules))
        {
            std::size_t maximum=0;
            for (const auto& rule:m_rules) {
                if (static_cast<unsigned>(rule.trigger)>static_cast<unsigned>(Trigger::WhileTrue))
                    throw std::invalid_argument("触发模式不受支持");
                if (rule.predicates.empty()) throw std::invalid_argument("规则必须有条件节点");
                maximum=std::max(maximum,rule.predicates.size());
                for (std::size_t i=0;i<rule.predicates.size();++i) {
                    const auto& p=rule.predicates[i];
                    if (static_cast<unsigned>(p.kind)>static_cast<unsigned>(PredicateKind::Not) ||
                        static_cast<unsigned>(p.comparison)>static_cast<unsigned>(Comparison::Outside) ||
                        static_cast<unsigned>(p.component)>static_cast<unsigned>(Component::Magnitude))
                        throw std::invalid_argument("条件类型或向量分量不受支持");
                    if (!std::isfinite(p.lower)||!std::isfinite(p.upper)||!std::isfinite(p.tolerance)||p.tolerance<0)
                        throw std::invalid_argument("条件阈值和容差必须有限且容差非负");
                    if ((p.comparison==Comparison::Inside || p.comparison==Comparison::Outside) && p.lower>p.upper)
                        throw std::invalid_argument("条件区间上下界颠倒");
                    if ((p.kind==PredicateKind::All || p.kind==PredicateKind::Any || p.kind==PredicateKind::Not) &&
                        (p.left>=i || (p.kind!=PredicateKind::Not && p.right>=i)))
                        throw std::invalid_argument("组合条件必须引用已定义节点");
                }
                for (const auto& command:rule.commands) {
                    if (static_cast<unsigned>(command.kind)>static_cast<unsigned>(CommandKind::SetForce))
                        throw std::invalid_argument("动作类型不受支持");
                    for (double value:command.value) if (!std::isfinite(value))
                        throw std::invalid_argument("动作包含非有限数值");
                }
            }
            m_values.resize(maximum); m_previous.resize(m_rules.size()); m_fired.resize(m_rules.size());
        }
        /** @brief 在同一输入快照上求值并输出动作，动作效果不影响本轮其它规则。 */
        void evaluate(const Frame& frame,CommandBuffer& output) override
        {
            for (std::size_t i=0;i<m_rules.size();++i) {
                const auto& rule=m_rules[i];
                if (rule.trigger==Trigger::Once && m_fired[i]) continue;
                for (std::size_t j=0;j<rule.predicates.size();++j) m_values[j]=evaluatePredicate(rule.predicates[j],frame);
                const bool current=m_values[rule.predicates.size()-1];
                const bool fire=current && (rule.trigger==Trigger::WhileTrue ||
                    (rule.trigger==Trigger::Once ? !m_fired[i] : !m_previous[i]));
                if (fire) {for (const auto& command:rule.commands) output.append(command); m_fired[i]=true;}
                m_previous[i]=current;
            }
        }
        /** @brief 清空一次触发与边沿检测历史。 */
        void reset() override {std::fill(m_previous.begin(),m_previous.end(),false);std::fill(m_fired.begin(),m_fired.end(),false);}
    private:
        /** @brief 比较有限数值，拒绝非有限状态。 */
        static bool compare(double value,const Predicate& p)
        {
            if (!std::isfinite(value)) return false;
            switch(p.comparison) {
            case Comparison::Less:return value<p.lower;
            case Comparison::LessEqual:return value<=p.lower;
            case Comparison::Greater:return value>p.lower;
            case Comparison::GreaterEqual:return value>=p.lower;
            case Comparison::Near:return std::abs(value-p.lower)<=p.tolerance;
            case Comparison::Inside:return value>=p.lower && value<p.upper;
            case Comparison::Outside:return value<p.lower || value>=p.upper;
            }
            return false;
        }
        /** @brief 从三维物理向量取出标量。 */
        static double scalar(const Vector& value,Component component)
        {
            if (component==Component::Magnitude) return std::hypot(value[0],value[1],value[2]);
            return value[static_cast<std::size_t>(component)];
        }
        /** @brief 求单个原子或组合节点，所有相对向量统一为第一物体减第二物体。 */
        bool evaluatePredicate(const Predicate& p,const Frame& frame) const
        {
            switch(p.kind) {
            case PredicateKind::Always:return true;
            case PredicateKind::All:return m_values[p.left] && m_values[p.right];
            case PredicateKind::Any:return m_values[p.left] || m_values[p.right];
            case PredicateKind::Not:return !m_values[p.left];
            case PredicateKind::Time:return compare(frame.time,p);
            default:break;
            }
            const auto* a=frame.find(p.first); if (!a) return false;
            if (p.kind==PredicateKind::Active) return a->active==p.expected;
            if (!a->active) return false;
            if (p.kind==PredicateKind::Asleep) return a->asleep==p.expected;
            const auto* b=frame.find(p.second);
            const bool relative=p.kind==PredicateKind::RelativePosition || p.kind==PredicateKind::RelativeVelocity ||
                p.kind==PredicateKind::RelativeAngularVelocity || p.kind==PredicateKind::Distance || p.kind==PredicateKind::Contact;
            if (relative && (!b || !b->active)) return false;
            if (p.kind==PredicateKind::Contact) return frame.touching(p.first,p.second)==p.expected;
            Vector value{};
            if (p.kind==PredicateKind::Position || p.kind==PredicateKind::RelativePosition || p.kind==PredicateKind::Distance) {
                value=a->position; if (relative) for(int k=0;k<3;++k) value[k]-=b->position[k];
            } else if (p.kind==PredicateKind::Velocity || p.kind==PredicateKind::RelativeVelocity) {
                value=a->velocity; if (relative) for(int k=0;k<3;++k) value[k]-=b->velocity[k];
            } else if (p.kind==PredicateKind::AngularVelocity || p.kind==PredicateKind::RelativeAngularVelocity) {
                value=a->angularVelocity; if (relative) for(int k=0;k<3;++k) value[k]-=b->angularVelocity[k];
            } else return false;
            return compare(scalar(value,p.kind==PredicateKind::Distance ? Component::Magnitude:p.component),p);
        }
        std::vector<Rule> m_rules;
        std::vector<bool> m_values,m_previous,m_fired;
    };
}
