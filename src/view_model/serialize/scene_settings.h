/** @file scene_settings.h @brief 场景显示与可视化条件规则的共享校验、编译入口。 */
#pragma once
#include "control/predicate_program.h"
#include "control_core/control_program.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <magic_enum.hpp>

namespace egret
{
    /** @brief 校验后的场景设置与原生规则，JSON 仅在加载或编辑时解析。 */
    struct ParsedSceneSettings
    {
        double referenceLength{1};
        bool markers{true};
        std::vector<control::Rule> rules;
        std::vector<control::ControlProgramSpec> programs;
    };
    /** @brief 读取有限数值，明确拒绝字符串及非有限参数。 */
    inline double settingNumber(const QJsonObject& object,const char* key,double fallback=0)
    {
        const auto value=object.value(QLatin1String(key));
        if(value.isUndefined()) return fallback;
        if(!value.isDouble() || !std::isfinite(value.toDouble())) throw std::invalid_argument("数值参数必须为有限数字");
        return value.toDouble();
    }
    /** @brief 将持久化英文标识转换为枚举，避免依赖枚举序号。 */
    template<class T> inline T settingEnum(const QJsonObject& object,const char* key,T fallback)
    {
        const auto value=object.value(QLatin1String(key));
        if(value.isUndefined()) return fallback;
        if(!value.isString()) throw std::invalid_argument("规则类型必须为名称字符串");
        const auto result=magic_enum::enum_cast<T>(value.toString().toStdString());
        if(!result) throw std::invalid_argument("规则中存在不支持的类型名称");
        return *result;
    }
    /** @brief 读取字符串形式的无符号稳定实体标识。 */
    inline control::EntityId settingEntity(const QJsonObject& object,const char* key)
    {
        bool ok=false;const auto value=object.value(QLatin1String(key));
        const auto result=value.toString().toULongLong(&ok);
        if(!value.isString() || !ok) throw std::invalid_argument("请选择有效的目标物体");
        return result;
    }
    /** @brief 将表单规则编译为原生谓词，支持逐项否定和全部／任一条件组合。 */
    inline ParsedSceneSettings parseSceneSettings(const QJsonObject& settings)
    {
        using namespace control;
        ParsedSceneSettings result;
        if(settings.contains("programs")&&!settings["programs"].isArray())throw std::invalid_argument("控制程序必须为数组");
        result.programs=control::parseProgramSpecs(QJsonDocument(settings["programs"].toArray()).toJson(QJsonDocument::Compact).toStdString());
        if(settings.contains("display") && !settings["display"].isObject()) throw std::invalid_argument("显示设置必须为对象");
        const auto display=settings["display"].toObject();
        result.referenceLength=settingNumber(display,"reference_length",1);
        if(result.referenceLength<1e-12 || result.referenceLength>1e12) throw std::invalid_argument("参考长度应在 10⁻¹² 至 10¹² 米之间");
        if(display.contains("markers") && !display["markers"].isBool()) throw std::invalid_argument("辅助标记开关必须为布尔值");
        result.markers=display["markers"].toBool(true);
        if(settings.contains("rules") && !settings["rules"].isArray()) throw std::invalid_argument("控制规则必须为数组");
        const auto rules=settings["rules"].toArray();
        if(rules.size()>256) throw std::invalid_argument("场景最多支持 256 条控制规则");
        std::size_t totalCommands=0;
        for(const auto& value:rules) {
            if(!value.isObject()) throw std::invalid_argument("规则必须为对象");
            const auto object=value.toObject();
            if(!object.value("conditions").isArray() || !object.value("actions").isArray())
                throw std::invalid_argument("规则必须包含条件和动作数组");
            const auto conditions=object["conditions"].toArray(), actions=object["actions"].toArray();
            if(conditions.isEmpty() || conditions.size()>128 || actions.isEmpty() || actions.size()>64)
                throw std::invalid_argument("每条规则需要 1～128 个条件及 1～64 个动作");
            const QString match=object.value("match").toString("All");
            if(match!="All" && match!="Any") throw std::invalid_argument("条件组合必须为全部或任一");
            Rule rule;rule.trigger=settingEnum(object,"trigger",Trigger::Once);
            std::size_t root=0;bool hasRoot=false;
            for(const auto& item:conditions) {
                if(!item.isObject()) throw std::invalid_argument("条件必须为对象");
                const auto c=item.toObject();Predicate p;
                p.kind=settingEnum(c,"kind",PredicateKind::Time);
                if(p.kind==PredicateKind::All || p.kind==PredicateKind::Any || p.kind==PredicateKind::Not)
                    throw std::invalid_argument("请使用规则的条件组合与取反开关");
                if(p.kind!=PredicateKind::Time && p.kind!=PredicateKind::Always) p.first=settingEntity(c,"first");
                if(p.kind==PredicateKind::RelativePosition || p.kind==PredicateKind::RelativeVelocity ||
                   p.kind==PredicateKind::RelativeAngularVelocity || p.kind==PredicateKind::Distance || p.kind==PredicateKind::Contact)
                    p.second=settingEntity(c,"second");
                p.component=settingEnum(c,"component",Component::Magnitude);
                p.comparison=settingEnum(c,"comparison",Comparison::GreaterEqual);
                p.lower=settingNumber(c,"lower");p.upper=settingNumber(c,"upper");p.tolerance=settingNumber(c,"tolerance",1e-6);
                if(c.contains("expected") && !c["expected"].isBool()) throw std::invalid_argument("状态期望必须为布尔值");
                p.expected=c.value("expected").toBool(true);
                rule.predicates.push_back(p);std::size_t current=rule.predicates.size()-1;
                if(c.value("negate").toBool(false)){Predicate n;n.kind=PredicateKind::Not;n.left=current;rule.predicates.push_back(n);current=rule.predicates.size()-1;}
                if(hasRoot) {
                    // 以已建立的根与当前条件构造新的根，避免用户操作节点下标。
                    Predicate combination;combination.kind=match=="All"?PredicateKind::All:PredicateKind::Any;
                    combination.left=root;combination.right=current;rule.predicates.push_back(combination);root=rule.predicates.size()-1;
                } else root=current;
                hasRoot=true;
            }
            for(const auto& item:actions) {
                if(!item.isObject()) throw std::invalid_argument("动作必须为对象");
                const auto a=item.toObject();Command command;
                command.kind=settingEnum(a,"kind",CommandKind::SetForce);command.target=settingEntity(a,"target");
                command.enabled=a.value("enabled").toBool(true);
                if(command.kind!=CommandKind::SetActive) {
                    if(!a["value"].isArray() || a["value"].toArray().size()!=3) throw std::invalid_argument("动作需要三个向量分量");
                    const auto vector=a["value"].toArray();
                    for(int k=0;k<3;++k){if(!vector[k].isDouble() || !std::isfinite(vector[k].toDouble())) throw std::invalid_argument("动作向量必须为有限数字");command.value[k]=vector[k].toDouble();}
                }
                rule.commands.push_back(command);
            }
            PredicateProgram validated({rule});
            if(object.value("enabled").toBool(true)) {totalCommands+=rule.commands.size();result.rules.push_back(std::move(rule));}
        }
        if(totalCommands>1024) throw std::invalid_argument("启用规则的动作总数不能超过 1024");
        return result;
    }
    /** @brief 为新建及旧版场景补默认源码，保留其他设置。 */
    inline QJsonObject normalizeSceneSettings(QJsonObject settings){
        if(!settings.contains("programs")||settings["programs"].toArray().isEmpty()){
            control::ControlProgramSpec spec;settings["programs"]=QJsonArray{QJsonObject{{"version",1},{"name",QString::fromStdString(spec.name)},{"source",QString::fromStdString(spec.source)},{"enabled",true},{"bindings",QJsonObject{}},{"period",0.0},{"start",0.0},{"fuel",1000000}}};
        }return settings;
    }
}
