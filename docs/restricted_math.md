<!-- @file restricted_math.md @brief 受限 C 数学库使用与预编译依赖说明。 -->
# 受限 C 数学库

控制器和可编程力场均可显式包含 `#include <math.h>`（也支持双引号）。此文件与 `egret_control.h` 并列，只公开数值函数，不开放系统标准库。

公开圆周率常量 `pi`，可直接写 `sin(pi / 2)`。

提供双精度三角、反三角、双曲、指数、对数、开方、取整、余数及 `erf/erfc/tgamma/lgamma` 等函数，完整列表见 SDK 的 `math.h`。函数遵循数学库浮点语义，定义域错误可能返回 NaN，溢出可能返回 Inf；提交控制动作、返回场力时仍拒绝非有限值。旧 `math_sqrt` 保留负数 trap 的兼容行为；旧 `math_cos` 已改用库实现。

实现取自官方 WASI SDK 27.0 的 wasi-libc（musl 数学实现），不是 Windows 本机库。运行 `tools/setup_control_math.ps1` 下载固定版本并核验 SHA256，复制预编译归档到 `src/control_core/sdk/libegret_math.a`，随原有 SDK 目录复制和部署。上游来源：https://github.com/WebAssembly/wasi-sdk/releases/tag/wasi-sdk-27 。对应上游提交为 `3f7eb4c7d6ede4dde3c4bffa6ed14e8d656fe93f`；第三方许可保存在 SDK 的 `math-LICENSE*.txt` 与 `math-COPYRIGHT.txt`，随 SDK 一同部署。

归档保留上游 libc.a 的原始内容，由静态链接器仅提取实际依赖的对象；公开头文件和 AST 白名单仅允许数学接口。没有启用 WASI 宿主导入。最终 Wasm 继续接受原有内存和 fuel 限制。无需编译数学源码；每份新用户程序仍需链接一次数学对象并由 Wasmtime 编译，已有程序缓存可复用。缓存键包含数学头文件及整个归档内容，升级库后旧缓存自然失效。

示例：

```c
#include <math.h>
/** @brief 返回随时间变化并在空间衰减的力。 */
Vector3 field_force(double t, Vector3 p) {
    return vector3(sin(t) * exp(-p.x*p.x), erf(p.y), cos(t));
}
/** @brief 示例作用域包含所有点。 */
bool field_contains(Vector3 p) { return true; }
```
