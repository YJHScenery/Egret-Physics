/** @file control_program.cpp @brief 原生受控编译进程、Clang AST 语义约束及 Wasmtime 固定资源执行。 */
#include "control_program.h"
#include "control_abi.h"
#include <json.hpp>
#include <wasmtime.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <mutex>
#include <regex>
#include <set>
#include <sstream>

namespace egret::control
{
namespace
{
using Json = nlohmann::json;
/** @brief 路径参数统一使用 UTF-8，支持中文安装目录与 Windows 用户目录。 */
std::string utf8(const std::filesystem::path &path)
{
    auto value = path.u8string();
    return std::string(value.begin(), value.end());
}
/** @brief 读取有大小上限的二进制文件。 */
std::string read(const std::filesystem::path &path, std::size_t limit = 32 * 1024 * 1024)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("无法读取文件：" + path.string());
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    if (size < 0 || static_cast<std::uint64_t>(size) > limit)
        throw std::runtime_error("编译文件超出大小限制");
    std::string value(static_cast<std::size_t>(size), '\0');
    file.seekg(0);
    file.read(value.data(), value.size());
    return value;
}
/** @brief 写入完整源码文件。 */
void write(const std::filesystem::path &path, const std::string &value)
{
    std::ofstream file(path, std::ios::binary);
    file.write(value.data(), value.size());
    if (!file)
        throw std::runtime_error("无法写入编译文件");
}
/** @brief 将 UTF-8 转为 Windows 原生命令参数。 */
std::wstring wide(const std::string &text)
{
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!n && !text.empty())
        throw std::runtime_error("命令参数不是有效 UTF-8");
    std::wstring result(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), n);
    return result;
}
/** @brief 按 Windows 参数规则转义，禁止经由 shell 执行用户文本。 */
std::wstring quote(const std::wstring &value)
{
    std::wstring out = L"\"";
    unsigned slash = 0;
    for (auto c : value)
    {
        if (c == L'\\')
        {
            slash++;
            continue;
        }
        out.append(c == L'"' ? slash * 2 + 1 : slash, L'\\');
        slash = 0;
        out += c;
    }
    out.append(slash * 2, L'\\');
    return out + L"\"";
}
/** @brief 原生句柄所有权，所有退出路径自动释放。 */
struct Handle
{
    HANDLE value{}; /** @brief 关闭有效句柄。 */
    ~Handle()
    {
        if (value && value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
};
/** @brief 独立编译进程；作业对象约束内存、取消、输出体积及总时长。 */
void process(const std::filesystem::path &executable, const std::vector<std::string> &args,
             const std::filesystem::path &output, const std::filesystem::path &errors, const CompilerOptions &options)
{
    if (options.shouldCancel && options.shouldCancel())
        throw std::runtime_error("控制编译已取消");
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    Handle out{CreateFileW(output.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr)};
    Handle err{CreateFileW(errors.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr)};
    Handle input{CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, nullptr)};
    Handle job{CreateJobObjectW(nullptr, nullptr)};
    if (out.value == INVALID_HANDLE_VALUE || err.value == INVALID_HANDLE_VALUE || !job.value)
        throw std::runtime_error("无法创建编译进程资源");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    limits.ProcessMemoryLimit = 1024ULL * 1024 * 1024;
    if (!SetInformationJobObject(job.value, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
        throw std::runtime_error("无法限制编译进程");
    std::wstring command = quote(executable.wstring());
    for (const auto &arg : args)
        command += L" " + quote(wide(arg));
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = out.value;
    startup.hStdError = err.value;
    startup.hStdInput = input.value;
    PROCESS_INFORMATION info{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
                        nullptr, nullptr, &startup, &info))
        throw std::runtime_error("无法启动 Clang，请配置控制工具链路径");
    Handle child{info.hProcess}, thread{info.hThread};
    if (!AssignProcessToJobObject(job.value, child.value))
    {
        TerminateProcess(child.value, 1);
        throw std::runtime_error("无法隔离编译进程");
    }
    ResumeThread(thread.value);
    const auto start = std::chrono::steady_clock::now();
    while (WaitForSingleObject(child.value, 25) == WAIT_TIMEOUT)
    {
        std::error_code ec;
        auto bytes = std::filesystem::file_size(output, ec);
        auto diagnostic = std::filesystem::file_size(errors, ec);
        if ((options.shouldCancel && options.shouldCancel()) ||
            std::chrono::steady_clock::now() - start > std::chrono::seconds(30) || bytes > 32 * 1024 * 1024 ||
            diagnostic > 1024 * 1024)
        {
            TerminateJobObject(job.value, 1);
            WaitForSingleObject(child.value, 1000);
            throw std::runtime_error("控制编译已取消、超时或输出超限");
        }
    }
    DWORD code = 1;
    GetExitCodeProcess(child.value, &code);
    if (code)
        throw std::runtime_error("C 编译失败：\n" + read(errors, 1024 * 1024));
}
/** @brief 计算带工具链版本及 SDK 内容的 SHA-256 缓存键。 */
std::string digest(const std::string &value)
{
    unsigned char bytes[32];
    if (BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char *>(value.data())),
                   static_cast<ULONG>(value.size()), bytes, 32))
        throw std::runtime_error("无法计算源码摘要");
    static const char hex[] = "0123456789abcdef";
    std::string out;
    for (auto b : bytes)
    {
        out += hex[b >> 4];
        out += hex[b & 15];
    }
    return out;
}
/** @brief 临时编译目录，在异常路径也清理。 */
struct Work
{
    std::filesystem::path path; /** @brief 仅清理本对象创建的专用目录。 */
    ~Work()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};
/** @brief 从单一 SDK 头文件读取允许函数清单。 */
std::set<std::string> sdkFunctions(const std::string &header)
{
    std::set<std::string> result;
    std::regex declaration(R"(\b([a-z][a-z_0-9]*)\s*\([^;{}]*\)\s*;)");
    for (auto it = std::sregex_iterator(header.begin(), header.end(), declaration); it != std::sregex_iterator(); ++it)
        result.insert((*it)[1]);
    return result;
}
/** @brief 校验用户 AST，而非用禁用词判断语言语义。 */
/** @brief 语义检查结果与准备阶段分析出的订阅。 */
struct ValidatedSource
{
    std::string source;
    bool readAll{}, noOp{}, contacts{};
};
ValidatedSource validateAst(const Json &ast, const std::set<std::string> &sdk, const std::set<std::string> &math, const ControlProgramSpec &spec)
{
    ValidatedSource validated;
    std::set<std::string> functions, definitions;
    std::map<std::string, std::set<std::string>> calls;
    /** @brief 仅对通过语义校验的调用进行可信领域降级。 */
    struct Replacement
    {
        std::size_t offset, length;
        std::string text;
    };
    std::vector<Replacement> replacements;
    std::map<std::string, unsigned> stateSlots;
    /** @brief 根据 Clang 源码范围登记替换，不通过文本搜索改变语义。 */
    auto replace = [&](const Json &node, std::string text) {
        const auto begin = node.at("range").at("begin"), end = node.at("range").at("end");
        auto offset = begin.at("offset").get<std::size_t>();
        auto finish = end.at("offset").get<std::size_t>() + end.at("tokLen").get<std::size_t>();
        if (finish > spec.source.size() || finish < offset)
            throw std::runtime_error("控制源码范围无效");
        replacements.push_back({offset, finish - offset, std::move(text)});
    };
    /** @brief 标识由强制包含的可信 SDK 提供的顶层声明。 */
    auto trusted = [](const Json &node) {
        return node.contains("loc") && (node["loc"].contains("includedFrom") || !node["loc"].contains("offset"));
    };
    for (const auto &node : ast.value("inner", Json::array()))
        if (!trusted(node) && node.value("kind", "") == "FunctionDecl")
        {
            functions.insert(node.value("name", ""));
            for (const auto &child : node.value("inner", Json::array()))
                if (child.value("kind", "") == "CompoundStmt")
                    definitions.insert(node.value("name", ""));
        }
    bool entry = false;
    std::function<void(const Json &, const std::string &, const std::string &)> visit;
    visit = [&](const Json &node, const std::string &function, const std::string &parent) {
        const auto kind = node.value("kind", "");
        auto type = node.value("type", Json::object()).value("qualType", "");
        if (kind.ends_with("Comment"))
            return;
        /** @brief 提供可定位的语义诊断。 */
        auto reject = [&](const std::string &message) {
            auto location = node.value("range", Json::object()).value("begin", Json::object());
            auto offset = std::min<std::size_t>(location.value("offset", 0), spec.source.size());
            auto line = 1 + std::count(spec.source.begin(), spec.source.begin() + offset, '\n');
            auto previous = spec.source.rfind('\n', offset ? offset - 1 : 0);
            auto column = previous == std::string::npos ? offset + 1 : offset - previous;
            throw std::runtime_error(spec.name + "：" + std::to_string(line) + ":" + std::to_string(column) +
                                     "：受限 C 校验失败：" + message);
        };
        static const std::set<std::string> allowed = {"FunctionDecl",
                                                      "VarDecl",
                                                      "ParmVarDecl",
                                                      "FieldDecl",
                                                      "RecordDecl",
                                                      "TypedefDecl",
                                                      "EnumDecl",
                                                      "EnumConstantDecl",
                                                      "CompoundStmt",
                                                      "DeclStmt",
                                                      "IfStmt",
                                                      "ForStmt",
                                                      "WhileStmt",
                                                      "DoStmt",
                                                      "ReturnStmt",
                                                      "BreakStmt",
                                                      "ContinueStmt",
                                                      "NullStmt",
                                                      "SwitchStmt",
                                                      "CaseStmt",
                                                      "DefaultStmt",
                                                      "IntegerLiteral",
                                                      "FloatingLiteral",
                                                      "CharacterLiteral",
                                                      "StringLiteral",
                                                      "DeclRefExpr",
                                                      "ImplicitCastExpr",
                                                      "CallExpr",
                                                      "UnaryOperator",
                                                      "BinaryOperator",
                                                      "CompoundAssignOperator",
                                                      "ParenExpr",
                                                      "MemberExpr",
                                                      "ConditionalOperator",
                                                      "InitListExpr",
                                                      "ImplicitValueInitExpr",
                                                      "ConstantExpr",
                                                      "UnaryExprOrTypeTraitExpr",
                                                      "BuiltinType",
                                                      "ElaboratedType",
                                                      "RecordType",
                                                      "EnumType",
                                                      "TypedefType"};
        if (!kind.empty() && !allowed.contains(kind) && kind != "ExprWithCleanups" &&
            kind != "MaterializeTemporaryExpr" && kind != "EmptyDecl")
            reject("语法节点未列入受限 C 白名单：" + kind);
        if (spec.fieldSourceLength && kind == "VarDecl" && node.value("storageClass", "") == "static")
            reject("场函数不允许静态状态");
        if (kind == "ArraySubscriptExpr" || kind == "CStyleCastExpr" || kind == "GCCAsmStmt" ||
            kind == "AddrLabelExpr" || kind == "GotoStmt" || kind == "IndirectGotoStmt" || kind == "StmtExpr" ||
            kind == "VAArgExpr" || kind == "CompoundLiteralExpr")
            reject("不允许下标、转换、汇编或扩展表达式");
        if ((kind == "VarDecl" || kind == "ParmVarDecl" || kind == "FieldDecl") &&
            (type.find('*') != std::string::npos || type.find('[') != std::string::npos))
            reject("用户变量不允许指针或数组；请使用领域集合");
        if (kind == "RecordDecl" && node.value("tagUsed", "") != "struct")
            reject("不允许通过联合体重解释不透明句柄");
        if (kind == "TypedefDecl" &&
            (type.find('*') != std::string::npos || type.find('[') != std::string::npos ||
             type.find("Entity") != std::string::npos || type.find("NumberList") != std::string::npos))
            reject("不允许指针、数组或不透明句柄类型别名");
        if (kind == "UnaryOperator" && (node.value("opcode", "") == "*" || node.value("opcode", "") == "&"))
            reject("用户不得操作地址；请使用领域集合值");
        if (kind == "MemberExpr")
        {
            auto children = node.value("inner", Json::array());
            auto base =
                children.empty() ? std::string{} : children[0].value("type", Json::object()).value("qualType", "");
            if (base == "Entity" || base == "EntityTemplate" || base == "EntityCollection" || base == "NumberList" ||
                node.value("name", "").rfind("_", 0) == 0)
                reject("不透明领域值不得访问内部字段");
        }
        if (kind == "InitListExpr" &&
            (type == "Entity" || type == "EntityTemplate" || type == "EntityCollection" || type == "NumberList"))
            reject("不允许伪造领域句柄");
        if (kind == "DeclRefExpr")
        {
            auto decl = node.value("referencedDecl", Json::object());
            if (decl.value("kind", "") == "FunctionDecl" && parent != "DirectCallee")
                reject("不允许函数指针");
        }
        if (kind == "CallExpr")
        {
            auto children = node.value("inner", Json::array());
            if (children.empty())
                reject("不允许间接调用");
            auto callee = children[0];
            while (callee.contains("inner") && callee["inner"].size() == 1)
                callee = callee["inner"][0];
            if (callee.value("kind", "") != "DeclRefExpr")
                reject("仅允许直接调用函数");
            std::string name = callee.value("referencedDecl", Json::object()).value("name", "");
            const auto callOffset = node.value("range", Json::object()).value("begin", Json::object()).value("offset", spec.source.size());
            if (spec.fieldSourceLength && callOffset < spec.fieldSourceLength && sdk.contains(name) &&
                name != "vector3" && name != "math_sqrt" && name != "math_abs" && name != "math_cos" &&
                name != "field_inside_box" && name != "field_inside_sphere" && !math.contains(name))
                reject("场函数仅允许使用无状态向量与数学接口");
            if (!sdk.contains(name) && !functions.contains(name))
                reject("函数未列入领域接口：" + name);
            if (name == "on_step")
                reject("不得手动调用控制入口");
            calls[function].insert(name);
            if (name == "scene_entities")
                validated.readAll = true;
            if (name == "entities_touching")
                validated.contacts = true;
            if (name == "scene_entity" || name == "scene_template" || name == "parameter_number" ||
                name.rfind("state_", 0) == 0)
            {
                if (children.size() < 2)
                    reject("缺少具名参数");
                auto literal = children[1];
                while (literal.contains("inner") && literal["inner"].size() == 1)
                    literal = literal["inner"][0];
                if (literal.value("kind", "") != "StringLiteral")
                    reject("实体、模板与状态的名称必须为字符串常量");
                auto value = literal.value("value", "");
                if (value.size() < 3 || value.size() > 65)
                    reject("名称长度应为 1～63 个 ASCII 字符");
                auto key = value.substr(1, value.size() - 2);
                if ((name == "scene_entity" || name == "scene_template") && !spec.bindings.contains(key))
                    reject("未绑定别名：" + key);
                if (name == "parameter_number")
                {
                    if (!spec.parameters.contains(key))
                        reject("缺失程序参数：" + key);
                    replace(node, Json(spec.parameters.at(key)).dump());
                }
                else if (name == "scene_entity" || name == "scene_template")
                    replace(node, "(" + std::string(name == "scene_entity" ? "Entity" : "EntityTemplate") + "){" +
                                      std::to_string(spec.bindings.at(key)) + "ULL}");
                else
                {
                    auto [it, inserted] = stateSlots.emplace(key, static_cast<unsigned>(stateSlots.size()));
                    if (stateSlots.size() > 256)
                        reject("具名状态超过 256 个");
                    replace(callee, "egret_abi_" + name);
                    replace(literal, std::to_string(it->second));
                }
            }
            // 直接调用的被调表达式不作为用户函数指针使用。
            for (std::size_t i = 1; i < children.size(); i++)
                visit(children[i], function, name == "collection_next" && i == 1 ? "CollectionArgument" : kind);
            return;
        }
        if (node.contains("inner"))
            for (const auto &child : node["inner"])
                visit(child, function, parent == "CollectionArgument" ? parent : kind);
    };
    for (const auto &node : ast.value("inner", Json::array()))
    {
        if (trusted(node))
            continue;
        const auto kind = node.value("kind", "");
        if (kind != "FunctionDecl" && kind != "VarDecl" && kind != "RecordDecl" && kind != "TypedefDecl" &&
            kind != "EnumDecl" && kind != "EmptyDecl")
            throw std::runtime_error("用户代码只能定义函数、数值结构与状态变量");
        if (spec.fieldSourceLength && kind == "VarDecl" && node.value("loc", Json::object()).value("offset", spec.source.size()) < spec.fieldSourceLength)
            throw std::runtime_error("场源码不得定义全局变量");
        auto name = node.value("name", "");
        if (name.rfind("egret_", 0) == 0 || name.rfind("__", 0) == 0 || sdk.contains(name) && name != "on_step")
            throw std::runtime_error("不得覆盖可信 SDK 名称：" + name);
        if (kind == "FunctionDecl")
        {
            bool defined = false;
            for (const auto &child : node.value("inner", Json::array()))
                if (child.value("kind", "") == "CompoundStmt")
                    defined = true;
            if (!definitions.contains(name))
                throw std::runtime_error("用户函数必须提供实现");
            if (name == "on_step" && defined)
            {
                if (node["type"].value("qualType", "") != "void (void)")
                    throw std::runtime_error("入口必须为 void on_step(void)");
                entry = true;
                for (const auto &child : node.value("inner", Json::array()))
                    if (child.value("kind", "") == "CompoundStmt" && !child.contains("inner"))
                        validated.noOp = true;
            }
        }
        visit(node, name, "");
    }
    if (!entry)
        throw std::runtime_error("必须定义 void on_step(void)");
    std::set<std::string> active, done;
    std::function<void(const std::string &)> check = [&](const std::string &name) {
        if (active.contains(name))
            throw std::runtime_error("用户函数不允许递归：" + name);
        if (done.contains(name))
            return;
        active.insert(name);
        for (const auto &target : calls[name])
            if (functions.contains(target))
                check(target);
        active.erase(name);
        done.insert(name);
    };
    for (const auto &name : functions)
        check(name);
    std::sort(replacements.begin(), replacements.end(),
              /** @brief 从尾向前替换，保持 Clang 字节偏移有效。 */ [](const auto &a, const auto &b) {
                  return a.offset > b.offset;
              });
    std::string lowered = spec.source;
    for (const auto &replacement : replacements)
        lowered.replace(replacement.offset, replacement.length, replacement.text);
    validated.source =
        "extern double egret_abi_state_number(unsigned);\nextern void "
        "egret_abi_state_set_number(unsigned,double);\nextern bool egret_abi_state_bool(unsigned);\nextern void "
        "egret_abi_state_set_bool(unsigned,bool);\nextern Entity egret_abi_state_entity(unsigned);\nextern void "
        "egret_abi_state_set_entity(unsigned,Entity);\n#line 1 \"scene_control.c\"\n" +
        lowered;
    return validated;
}
/** @brief 将 Wasmtime 错误与 trap 转为中文上下文异常并释放资源。 */
void checked(wasmtime_error_t *error, wasm_trap_t *trap = nullptr)
{
    wasm_byte_vec_t message{};
    if (error)
    {
        wasmtime_error_message(error, &message);
        wasmtime_error_delete(error);
    }
    else if (trap)
    {
        wasm_trap_message(trap, &message);
        wasm_trap_delete(trap);
    }
    else
        return;
    std::string text(message.data, message.size);
    wasm_byte_vec_delete(&message);
    throw std::runtime_error("控制执行失败（越界、预算耗尽或无效实体）：" + text);
}
/** @brief 明确逐字段转换，避免假定 C++ 快照与 Wasm ABI 同布局。 */
EgretBody packBody(const BodyState &body)
{
    EgretBody b{};
    b.id = body.id;
    b.position = {body.position[0], body.position[1], body.position[2]};
    b.velocity = {body.velocity[0], body.velocity[1], body.velocity[2]};
    b.angular = {body.angularVelocity[0], body.angularVelocity[1], body.angularVelocity[2]};
    b.active = body.active;
    b.mass = body.mass;
    b.rotatable = body.rotatable;
    b.rotation = {body.rotation[0], body.rotation[1], body.rotation[2], body.rotation[3]};
    b.material = {body.material[0], body.material[1], body.material[2]};
    return b;
}
/** @brief 固定内存、无宿主导入的 Wasm 后端。 */
class WasmProgram final : public Program
{
  public:
    /** @brief 编译模块并建立世界独立状态。 */
    WasmProgram(ControlProgramSpec spec, const std::string &bytes, unsigned identity, bool readAll, bool noOp,
                bool contacts, PreparationReport &report)
        : m_spec(std::move(spec)), m_identity(identity), m_readAll(readAll), m_noOp(noOp), m_wantsContacts(contacts)
    {
        auto started = std::chrono::steady_clock::now();
        for (const auto &[name, id] : m_spec.bindings)
            m_observed.push_back(id);
        std::sort(m_observed.begin(), m_observed.end());
        auto config = wasm_config_new();
        wasmtime_config_consume_fuel_set(config, true);
        wasmtime_config_wasm_threads_set(config, false);
        wasmtime_config_wasm_memory64_set(config, false);
        wasmtime_config_wasm_multi_memory_set(config, false);
        m_engine = wasm_engine_new_with_config(config);
        try
        {
            checked(wasmtime_module_new(m_engine, reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size(),
                                        &m_module));
            report.machineCompileSeconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            started = std::chrono::steady_clock::now();
            wasm_importtype_vec_t imports;
            wasmtime_module_imports(m_module, &imports);
            bool empty = imports.size == 0;
            wasm_importtype_vec_delete(&imports);
            if (!empty)
                throw std::runtime_error("控制模块不得导入宿主函数");
            reset();
            wasm_exporttype_vec_t exports;
            wasmtime_module_exports(m_module, &exports);
            bool validExports = exports.size == 3;
            for (std::size_t i = 0; i < exports.size; ++i)
            {
                auto name = wasm_exporttype_name(exports.data[i]);
                std::string value(name->data, name->size);
                if (value != "memory" && value != "egret_step" && value != "egret_exchange")
                    validExports = false;
            }
            wasm_exporttype_vec_delete(&exports);
            if (!validExports)
                throw std::runtime_error("控制模块导出不符合固定 ABI");
            report.instanceSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        }
        catch (...)
        {
            cleanup();
            throw;
        }
    }
    /** @brief 释放实例、模块和引擎。 */
    ~WasmProgram() override
    {
        cleanup();
    }
    /** @brief 无需采样即可判断调度是否到期。 */
    bool due(double time) const override
    {
        return !m_noOp && time + 1e-12 >= m_next && time >= m_spec.start && time < m_spec.end;
    }
    /** @brief 仅订阅具名绑定、当前程序动态身份，显式遍历时订阅全部。 */
    bool observes(EntityId id) const override
    {
        return m_readAll || std::binary_search(m_observed.begin(), m_observed.end(), id) ||
               (m_prefix && (id & 0xffffffff00000000ULL) == m_prefix);
    }
    /** @brief 结束调度或无接触调用的程序不累计事件。 */
    bool wantsContacts(double time) const override
    {
        return !m_noOp && m_wantsContacts && time < m_spec.end;
    }
    /** @brief 空入口不引入多余物理力场记录。 */
    bool usesForces() const override
    {
        return !m_noOp;
    }
    /** @brief 场采样允许时刻倒序，并保持每次调用独立调度。 */
    void evaluateSample(const Frame& frame, CommandBuffer& output) override
    {
        if (!m_spec.fieldSourceLength) throw std::runtime_error("普通控制程序不能作为场采样");
        m_next = 0;
        evaluate(frame, output);
    }    /** @brief 重新实例化，恢复所有用户全局变量及状态池。 */
    void reset() override
    {
        if (m_store)
            wasmtime_store_delete(m_store);
        m_store = wasmtime_store_new(m_engine, nullptr, nullptr);
        auto context = wasmtime_store_context(m_store);
        // 预编译数学对象保留一个空表槽；用户函数指针仍由 AST 禁止。
        wasmtime_store_limiter(m_store, 2 * 1024 * 1024, 1, 1, 1, 1);
        checked(wasmtime_context_set_fuel(context, m_spec.fuel));
        wasmtime_instance_t instance;
        wasm_trap_t *trap = nullptr;
        auto creationError = wasmtime_instance_new(context, m_module, nullptr, 0, &instance, &trap);
        checked(creationError, trap);
        wasmtime_extern_t item;
        if (!wasmtime_instance_export_get(context, &instance, "memory", 6, &item) ||
            item.kind != WASMTIME_EXTERN_MEMORY)
            throw std::runtime_error("模块缺少内存导出");
        m_memory = item.of.memory;
        auto type = wasmtime_memory_type(context, &m_memory);
        uint64_t maximum = 0;
        bool fixed = wasmtime_memorytype_maximum(type, &maximum) && maximum == wasmtime_memorytype_minimum(type);
        wasm_memorytype_delete(type);
        if (!fixed || maximum != 32)
            throw std::runtime_error("控制内存必须固定为 32 页");
        if (!wasmtime_instance_export_get(context, &instance, "egret_step", 10, &item) ||
            item.kind != WASMTIME_EXTERN_FUNC)
            throw std::runtime_error("模块缺少入口");
        m_step = item.of.func;
        if (!wasmtime_instance_export_get(context, &instance, "egret_exchange", 14, &item) ||
            item.kind != WASMTIME_EXTERN_FUNC)
            throw std::runtime_error("模块缺少交换区");
        wasmtime_val_t result;
        trap = nullptr;
        auto error = wasmtime_func_call(context, &item.of.func, nullptr, 0, &result, 1, &trap);
        checked(error, trap);
        if (result.kind != WASMTIME_I32)
            throw std::runtime_error("交换区类型错误");
        m_offset = static_cast<uint32_t>(result.of.i32);
        if (m_offset > wasmtime_memory_data_size(context, &m_memory) ||
            sizeof(EgretExchange) > wasmtime_memory_data_size(context, &m_memory) - m_offset)
            throw std::runtime_error("交换区越界");
        m_next = m_spec.start;
        m_last = 0;
        m_first = true;
        m_failed = false;
        m_prefix = 0;
    }
    /** @brief 批量输入、fuel 执行与权威输出重校验。 */
    void evaluate(const Frame &frame, CommandBuffer &output) override
    {
        if (m_failed)
            throw std::runtime_error("控制程序上次已失败，请重置重跑");
        if (!due(frame.time))
            return;
        try
        {
            if (frame.bodies.size() > EGRET_BODY_CAPACITY || frame.templates.size() > EGRET_BODY_CAPACITY ||
                frame.contacts.size() > EGRET_CONTACT_CAPACITY)
                throw std::runtime_error("控制输入超过实体或接触容量");
            auto context = wasmtime_store_context(m_store);
            auto data = wasmtime_memory_data(context, &m_memory) + m_offset;
            // 宿主私有交换区不允许用户内存的数量字段反向影响校验。
            if (!m_prefix)
            {
                m_prefix = 0x8000000000000000ULL | (static_cast<uint64_t>(m_identity) << 32);
                bool collision = true;
                while (collision)
                {
                    collision = false;
                    for(auto id:m_spec.reservedIdentities)
                        if((id&0xffffffff00000000ULL)==m_prefix)collision=true;
                    for (const auto &body : frame.templates)
                        if ((body.id & 0xffffffff00000000ULL) == m_prefix)
                            collision = true;
                    for (const auto &body : frame.bodies)
                        if ((body.id & 0xffffffff00000000ULL) == m_prefix)
                            collision = true;
                    if (collision)
                    {
                        m_prefix += 0x1000000000000ULL;
                        if (!(m_prefix >> 63))
                            throw std::runtime_error("无法为动态实体分配独立身份区间");
                    }
                }
            }
            auto &input = *m_exchange;
            std::memset(&input, 0, m_spec.fieldSourceLength ? offsetof(EgretExchange, bindings) : sizeof(input));
            input.version = EGRET_ABI_VERSION;
            input.bytes = sizeof(input);
            input.time = frame.time;
            input.step = frame.step;
            input.delta = m_first ? 0 : frame.time - m_last;
            input.stepIndex = frame.stepIndex;
            input.identityPrefix = m_prefix;
            for (const auto &[name, id] : m_spec.bindings)
            {
                auto &b = input.bindings[input.bindingCount++];
                b.id = id;
                std::memcpy(b.name, name.c_str(), name.size() + 1);
            }
            for (const auto &body : frame.bodies)
                if (observes(body.id))
                    input.bodies[input.bodyCount++] = packBody(body);
            for (const auto &pair : frame.contacts)
                input.contacts[input.contactCount++] = {pair.first, pair.second};
            for (const auto &body : frame.templates)
                if (std::binary_search(m_observed.begin(), m_observed.end(), body.id))
                    input.templates[input.templateCount++] = packBody(body);
            if (m_spec.fieldSourceLength) {
                std::memcpy(data, &input, offsetof(EgretExchange, bindings));
                std::memcpy(data + offsetof(EgretExchange, bindings), input.bindings, input.bindingCount * sizeof(EgretBinding));
                std::memcpy(data + offsetof(EgretExchange, bodies), input.bodies, input.bodyCount * sizeof(EgretBody));
            } else {
                std::memcpy(data, &input, sizeof(input));
            }
            checked(wasmtime_context_set_fuel(context, m_spec.fuel));
            wasm_trap_t *trap = nullptr;
            auto error = wasmtime_func_call(context, &m_step, nullptr, 0, nullptr, 0, &trap);
            checked(error, trap);
            EgretU32 count;
            std::memcpy(&count, data + offsetof(EgretExchange, commandCount), sizeof(count));
            if (count > EGRET_COMMAND_CAPACITY)
                throw std::runtime_error("动作数量超过 ABI 容量");
            for (unsigned i = 0; i < count; i++)
            {
                EgretCommand c;
                std::memcpy(&c, data + offsetof(EgretExchange, commands) + i * sizeof(c), sizeof(c));
                if (c.kind > 11 || c.kind == 6 || c.enabled > 1)
                    throw std::runtime_error("非法控制动作");
                Command command;
                command.kind = static_cast<CommandKind>(c.kind);
                command.target = c.target;
                command.source = c.source;
                command.value = {c.value.x, c.value.y, c.value.z};
                command.enabled = c.enabled != 0;
                command.scalar = c.scalar;
                output.append(command);
            }
            m_last = frame.time;
            m_first = false;
            m_next = m_spec.period > 0
                         ? m_spec.start +
                               (std::floor((frame.time - m_spec.start) / m_spec.period + 1e-9) + 1) * m_spec.period
                         : frame.time;
        }
        catch (const std::exception &error)
        {
            m_failed = true;
            throw std::runtime_error(m_spec.name + "，步 " + std::to_string(frame.stepIndex) + "，仿真时间 " +
                                     std::to_string(frame.time) + " 秒：" + error.what());
        }
    }

  private:
    /** @brief 安全释放已创建的 Wasmtime 对象。 */
    void cleanup()
    {
        if (m_store)
            wasmtime_store_delete(m_store);
        if (m_module)
            wasmtime_module_delete(m_module);
        if (m_engine)
            wasm_engine_delete(m_engine);
        m_store = nullptr;
        m_module = nullptr;
        m_engine = nullptr;
    }
    ControlProgramSpec m_spec;
    unsigned m_identity;
    wasm_engine_t *m_engine{};
    wasmtime_module_t *m_module{};
    wasmtime_store_t *m_store{};
    wasmtime_memory_t m_memory{};
    wasmtime_func_t m_step{};
    uint32_t m_offset{};
    std::unique_ptr<EgretExchange> m_exchange = std::make_unique<EgretExchange>();
    double m_next{}, m_last{};
    bool m_first{true}, m_failed{};
    std::uint64_t m_prefix{};
    bool m_readAll{}, m_noOp{}, m_wantsContacts{};
    std::vector<EntityId> m_observed;
};
} // namespace
CompilerOptions defaultCompilerOptions()
{
    wchar_t path[32768];
    auto n = GetModuleFileNameW(nullptr, path, 32768);
    if (!n || n == 32768)
        throw std::runtime_error("无法定位应用目录");
    auto root = std::filesystem::path(path).parent_path() / L"control";
    CompilerOptions options{root / L"bin" / L"clang.exe",
                            root / L"sdk",
                            std::filesystem::temp_directory_path() / L"egret-control-wasm-v1",
                            {}};
    /** @brief 支持非 GUI 宿主显式指定部署目录。 */
    auto overridePath = [](const wchar_t *name, std::filesystem::path &target) {
        wchar_t value[32768];
        auto count = GetEnvironmentVariableW(name, value, 32768);
        if (count && count < 32768)
            target = value;
    };
    overridePath(L"EGRET_CONTROL_CLANG", options.clangPath);
    overridePath(L"EGRET_CONTROL_SDK", options.sdkDirectory);
    overridePath(L"EGRET_CONTROL_CACHE", options.cacheDirectory);
    return options;
}
std::unique_ptr<Program> prepareVectorFieldProgram(const std::string& source, const CompilerOptions& options,
                                                   std::uint64_t fuel)
{
    if (source.empty()) throw std::runtime_error("场源码不能为空");
    ControlProgramSpec spec;
    spec.name = "矢量力场";
    spec.fuel = fuel;
    spec.fieldSourceLength = source.size();
    spec.bindings["field_sample"] = 1;
    spec.source = source + R"(
void on_step(void) {
    Entity point = scene_entity("field_sample");
    Vector3 position = entity_position(point);
    Vector3 value = vector3(0, 0, 0);
    if (field_contains(position)) value = field_force(simulation_time(), position);
    entity_set_force(point, value);
}
)";
    return prepareProgram(spec, options);
}std::unique_ptr<Program> prepareProgram(const ControlProgramSpec &spec, const CompilerOptions &options,
                                        unsigned identityNamespace)
{
    try
    {
        PreparationReport report;
        report.name = spec.name;
        auto started = std::chrono::steady_clock::now();
        if (options.shouldCancel && options.shouldCancel())
            throw std::runtime_error("控制程序准备已取消");
        if (spec.source.empty() || spec.source.size() > 256 * 1024 || spec.bindings.size() > 256 || spec.fuel < 100 ||
            spec.fuel > 100000000 || !std::isfinite(spec.period) || spec.period < 0 || !std::isfinite(spec.start) ||
            spec.start < 0 || !std::isfinite(spec.end) || spec.end <= spec.start || identityNamespace == 0 ||
            identityNamespace > 65535)
            throw std::runtime_error("控制程序源码、调度或预算配置无效");
        for (const auto &[name, id] : spec.bindings)
            if (!std::regex_match(name, std::regex("[A-Za-z][A-Za-z0-9_]{0,62}")) || !id)
                throw std::runtime_error("别名须为 1～63 个英文字母、数字或下划线，且绑定原始场景实体");
        if (spec.parameters.size() > 256)
            throw std::runtime_error("程序参数超过 256 个");
        for (const auto &[name, value] : spec.parameters)
            if (!std::regex_match(name, std::regex("[A-Za-z][A-Za-z0-9_]{0,62}")) || !std::isfinite(value))
                throw std::runtime_error("程序参数必须为英文名称与有限数字");
        if (spec.backend == "lua") {
            report.digest=digest("egret-lua-5.4.8-sdk1-"+spec.source);
            auto program=prepareLuaProgram(spec,options,identityNamespace);
            report.instanceSeconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
            if(options.onPrepared)options.onPrepared(report);
            return program;
        }
        if (spec.backend != "c_wasm") throw std::runtime_error("不支持的控制后端");
        // 用户预处理仅支持公开 SDK，避免绝对路径 include、宏展开与工具链配置逃逸。
        std::istringstream lines(spec.source);
        std::string line;
        while (std::getline(lines, line))
            if (std::regex_search(line, std::regex(R"(^\s*#)")) &&
                !std::regex_match(line, std::regex(R"re(^\s*#\s*include\s*("egret_control\.h"|"math\.h"|<math\.h>)\s*)re")))
                throw std::runtime_error("仅允许包含 egret_control.h 或 math.h，不允许自定义预处理指令");
        std::filesystem::create_directories(options.cacheDirectory);
        std::wstring temp = L"compile-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                            std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count());
        Work work{options.cacheDirectory / temp};
        if (!std::filesystem::create_directory(work.path))
            throw std::runtime_error("无法创建独立编译目录");
        auto source = work.path / L"program.c", ast = work.path / L"ast.json", errors = work.path / L"diagnostics.txt",
             version = work.path / L"version.txt";
        write(source, spec.source);
        process(options.clangPath, {"--version"}, version, errors, options);
        auto header = read(options.sdkDirectory / L"egret_control.h");
        auto runtime = read(options.sdkDirectory / L"egret_runtime.c");
        auto mathHeader = read(options.sdkDirectory / L"math.h");
        auto mathLibrary = options.sdkDirectory / L"libegret_math.a";
        auto mathBytes = read(mathLibrary);
        auto abi = read(options.sdkDirectory.parent_path() / L"control_abi.h");
        std::vector<std::string> flags = {"--target=wasm32-unknown-unknown",
                                          "-std=c11",
                                          "-pedantic-errors",
                                          "-nostdinc",
                                          "-fno-builtin",
                                          "-fno-color-diagnostics",
                                          "-Werror",
                                          "-Wno-unused-value",
                                          "-I",
                                          utf8(options.sdkDirectory),
                                          "-include",
                                          utf8(options.sdkDirectory / L"egret_control.h")};
        // 使用 Clang 的实际预处理文件标记核验头文件白名单，不能只依赖源码中的指令外观。
        auto preprocessed = work.path / L"preprocessed.c";
        auto preprocessArgs = flags;
        preprocessArgs.insert(preprocessArgs.end(), {"-E", utf8(source)});
        process(options.clangPath, preprocessArgs, preprocessed, errors, options);
        std::istringstream expanded(read(preprocessed));
        std::regex marker(R"re(^#\s+[0-9]+\s+("(?:[^"\\]|\\.)*").*)re");
        while (std::getline(expanded, line))
        {
            std::smatch match;
            if (!std::regex_match(line, match, marker))
                continue;
            auto included = Json::parse(match[1].str()).get<std::string>();
            if (included == "<built-in>" || included == "<command line>")
                continue;
            std::error_code ec;
            auto file = std::filesystem::weakly_canonical(std::filesystem::u8path(included), ec);
            if (ec || !(std::filesystem::equivalent(file, source, ec) ||
                        std::filesystem::equivalent(file, options.sdkDirectory / L"egret_control.h", ec) ||
                        std::filesystem::equivalent(file, options.sdkDirectory / L"math.h", ec)))
                throw std::runtime_error("控制源码包含未授权头文件：" + included);
        }
        auto args = flags;
        args.insert(args.end(), {"-Xclang", "-ast-dump=json", "-fsyntax-only", utf8(source)});
        process(options.clangPath, args, ast, errors, options);
        auto lowered = validateAst(Json::parse(read(ast)), sdkFunctions(header + mathHeader), sdkFunctions(mathHeader), spec);
        write(source, lowered.source);
        std::string compilerFlags;
        for (const auto &flag : flags)
        {
            compilerFlags += flag;
            compilerFlags += '\0';
        }
        const auto key = digest(
            "egret-control-abi1-llvm21.1.8-wasmtime38.0.3-memory32-lowering1-O2-debug-noentry-export2-stack262144" +
            compilerFlags + read(version) + header + mathHeader + mathBytes + runtime + abi + spec.source + lowered.source);
        auto cached = options.cacheDirectory / (key + ".wasm");
        std::string bytes;
        report.digest = key;
        report.validationSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        started = std::chrono::steady_clock::now();
        if (std::filesystem::exists(cached))
        {
            bytes = read(cached, 4 * 1024 * 1024);
            report.cacheHit = true;
        }
        else
        {
            auto wasm = work.path / L"program.wasm";
            args = flags;
            args.insert(args.end(), {"-O2", "-g", "-nostdlib", "-Wl,--no-entry", "-Wl,--export=egret_step",
                                     "-Wl,--export=egret_exchange", "-Wl,--initial-memory=2097152",
                                     "-Wl,--max-memory=2097152", "-Wl,-z,stack-size=262144", utf8(source),
                                     utf8(options.sdkDirectory / L"egret_runtime.c"), utf8(mathLibrary), "-o", utf8(wasm)});
            process(options.clangPath, args, work.path / L"compile.txt", errors, options);
            bytes = read(wasm, 4 * 1024 * 1024);
            std::error_code ec;
            // 临时产物已关闭，使用同卷重命名原子发布，避免并行检查读到半个缓存文件。
            std::filesystem::rename(wasm, cached, ec);
        }
        // 缓存只保存沙箱 Wasm，绝不反序列化来源不明的本机机器码。
        report.cCompileSeconds =
            report.cacheHit ? 0 : std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        if (options.shouldCancel && options.shouldCancel())
            throw std::runtime_error("控制程序准备已取消");
        auto program = std::make_unique<WasmProgram>(spec, bytes, identityNamespace, lowered.readAll, lowered.noOp,
                                                     lowered.contacts, report);
        if (options.shouldCancel && options.shouldCancel())
            throw std::runtime_error("控制程序准备已取消");
        if (options.onPrepared)
            options.onPrepared(report);
        return program;
    }
    catch (const std::exception &error)
    {
        throw std::runtime_error("程序【" + spec.name + "】：" + error.what());
    }
}
bool CompositeProgram::due(double time) const
{
    for (const auto &program : m_programs)
        if (program->due(time))
            return true;
    return false;
}
bool CompositeProgram::observes(EntityId id) const
{
    for (const auto &program : m_programs)
        if (program->observes(id))
            return true;
    return false;
}
bool CompositeProgram::wantsContacts(double time) const
{
    for (const auto &program : m_programs)
        if (program->wantsContacts(time))
            return true;
    return false;
}
bool CompositeProgram::usesForces() const
{
    for (const auto &program : m_programs)
        if (program->usesForces())
            return true;
    return false;
}
void CompositeProgram::evaluate(const Frame &frame, CommandBuffer &output)
{
    for (std::size_t i = 0; i < m_programs.size(); ++i)
    {
        auto &contacts = m_contacts[i];
        if (m_programs[i]->wantsContacts(frame.time))
            contacts.insert(contacts.end(), frame.contacts.begin(), frame.contacts.end());
        else
            contacts.clear();
        std::sort(contacts.begin(), contacts.end(),
                  /** @brief 接触按稳定身份对排序。 */ [](const auto &a, const auto &b) {
                      return a.first < b.first || (a.first == b.first && a.second < b.second);
                  });
        contacts.erase(
            std::unique(contacts.begin(), contacts.end(), /** @brief 合并累计接触。 */
                        [](const auto &a, const auto &b) { return a.first == b.first && a.second == b.second; }),
            contacts.end());
        if (contacts.size() > 8192)
            throw std::runtime_error("程序累计接触超过预算");
        if (m_programs[i]->due(frame.time))
        {
            auto input = frame;
            input.contacts = contacts;
            m_programs[i]->evaluate(input, output);
            contacts.clear();
        }
    }
}
void CompositeProgram::reset()
{
    for (auto &program : m_programs)
        program->reset();
    for (auto &contacts : m_contacts)
        contacts.clear();
}
std::vector<ControlProgramSpec> parseProgramSpecs(const std::string &json)
{
    const auto root = Json::parse(json);
    if (!root.is_array() || root.size() > 32)
        throw std::runtime_error("场景最多支持 32 个控制程序");
    std::vector<ControlProgramSpec> result;
    for (const auto &item : root)
    {
        if (!item.is_object() || item.value("version", 1) != 1)
            throw std::runtime_error("不支持的控制程序版本");
        ControlProgramSpec spec;
        spec.backend = item.value("backend", std::string("c_wasm"));
        if (spec.backend != "c_wasm" && spec.backend != "lua") throw std::runtime_error("不支持的控制后端");
        if (spec.backend == "lua") spec.source = "-- 场景控制入口。\nfunction on_step()\nend\n";
        spec.name = item.value("name", spec.name);
        spec.source = item.value("source", spec.source);
        spec.enabled = item.value("enabled", true);
        spec.period = item.value("period", 0.0);
        spec.start = item.value("start", 0.0);
        spec.end = item.value("end", 1e100);
        spec.fuel = item.value("fuel", 1000000ULL);
        auto parameters = item.value("parameters", Json::object());
        if (!parameters.is_object() || parameters.size() > 256)
            throw std::runtime_error("程序参数必须为至多 256 项的对象");
        for (auto it = parameters.begin(); it != parameters.end(); ++it)
        {
            if (!it.value().is_number() || !std::regex_match(it.key(), std::regex("[A-Za-z][A-Za-z0-9_]{0,62}")))
                throw std::runtime_error("程序参数必须是具名数字");
            auto value = it.value().get<double>();
            if (!std::isfinite(value))
                throw std::runtime_error("程序参数必须有限");
            spec.parameters[it.key()] = value;
        }
        if (spec.name.size() > 256 || spec.source.empty() || spec.source.size() > 256 * 1024 ||
            !std::isfinite(spec.period) || spec.period < 0 || spec.start < 0 || !std::isfinite(spec.start) ||
            !std::isfinite(spec.end) || spec.end <= spec.start || spec.fuel < 100 || spec.fuel > 100000000)
            throw std::runtime_error("控制源码、周期、起止时间或 fuel 预算无效");
        auto bindings = item.value("bindings", Json::object());
        if (!bindings.is_object() || bindings.size() > 256)
            throw std::runtime_error("实体绑定必须是至多 256 个别名的对象");
        for (auto it = bindings.begin(); it != bindings.end(); ++it)
        {
            if (!std::regex_match(it.key(), std::regex("[A-Za-z][A-Za-z0-9_]{0,62}")) || !it.value().is_string())
                throw std::runtime_error("实体别名需使用英文命名，身份需使用十进制字符串");
            auto text = it.value().get<std::string>();
            if (text.empty() || text.size() > 20 || text.find_first_not_of("0123456789") != std::string::npos)
                throw std::runtime_error("非法实体身份");
            auto id = std::stoull(text);
            if (!id)
                throw std::runtime_error("绑定必须指向初始场景实体");
            spec.bindings.emplace(it.key(), id);
        }
        result.push_back(std::move(spec));
    }
    if (result.empty())
        result.emplace_back();
    return result;
}
} // namespace egret::control



