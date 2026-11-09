/** @file math.h @brief 受限 C 数学接口；实现来自预编译 wasi-libc（musl 数学库），遵循浮点 NaN/Inf 语义。 */
#pragma once
/** @brief 圆周率，展开为双精度数值常量。 */
#define pi 3.141592653589793238462643383279502884
/** @brief 调用数学库的 sin 双精度函数。 */
double sin(double value);
/** @brief 调用数学库的 cos 双精度函数。 */
double cos(double value);
/** @brief 调用数学库的 tan 双精度函数。 */
double tan(double value);
/** @brief 调用数学库的 asin 双精度函数。 */
double asin(double value);
/** @brief 调用数学库的 acos 双精度函数。 */
double acos(double value);
/** @brief 调用数学库的 atan 双精度函数。 */
double atan(double value);
/** @brief 调用数学库的 sinh 双精度函数。 */
double sinh(double value);
/** @brief 调用数学库的 cosh 双精度函数。 */
double cosh(double value);
/** @brief 调用数学库的 tanh 双精度函数。 */
double tanh(double value);
/** @brief 调用数学库的 asinh 双精度函数。 */
double asinh(double value);
/** @brief 调用数学库的 acosh 双精度函数。 */
double acosh(double value);
/** @brief 调用数学库的 atanh 双精度函数。 */
double atanh(double value);
/** @brief 调用数学库的 exp 双精度函数。 */
double exp(double value);
/** @brief 调用数学库的 exp2 双精度函数。 */
double exp2(double value);
/** @brief 调用数学库的 expm1 双精度函数。 */
double expm1(double value);
/** @brief 调用数学库的 log 双精度函数。 */
double log(double value);
/** @brief 调用数学库的 log2 双精度函数。 */
double log2(double value);
/** @brief 调用数学库的 log10 双精度函数。 */
double log10(double value);
/** @brief 调用数学库的 log1p 双精度函数。 */
double log1p(double value);
/** @brief 调用数学库的 sqrt 双精度函数。 */
double sqrt(double value);
/** @brief 调用数学库的 cbrt 双精度函数。 */
double cbrt(double value);
/** @brief 调用数学库的 fabs 双精度函数。 */
double fabs(double value);
/** @brief 调用数学库的 floor 双精度函数。 */
double floor(double value);
/** @brief 调用数学库的 ceil 双精度函数。 */
double ceil(double value);
/** @brief 调用数学库的 trunc 双精度函数。 */
double trunc(double value);
/** @brief 调用数学库的 round 双精度函数。 */
double round(double value);
/** @brief 调用数学库的 erf 双精度函数。 */
double erf(double value);
/** @brief 调用数学库的 erfc 双精度函数。 */
double erfc(double value);
/** @brief 调用数学库的 tgamma 双精度函数。 */
double tgamma(double value);
/** @brief 调用数学库的 lgamma 双精度函数。 */
double lgamma(double value);
/** @brief 调用数学库的 atan2 双精度二元函数。 */
double atan2(double first, double second);
/** @brief 调用数学库的 pow 双精度二元函数。 */
double pow(double first, double second);
/** @brief 调用数学库的 hypot 双精度二元函数。 */
double hypot(double first, double second);
/** @brief 调用数学库的 fmod 双精度二元函数。 */
double fmod(double first, double second);
/** @brief 调用数学库的 remainder 双精度二元函数。 */
double remainder(double first, double second);
/** @brief 调用数学库的 copysign 双精度二元函数。 */
double copysign(double first, double second);
/** @brief 调用数学库的 fmin 双精度二元函数。 */
double fmin(double first, double second);
/** @brief 调用数学库的 fmax 双精度二元函数。 */
double fmax(double first, double second);
/** @brief 调用数学库的 fdim 双精度二元函数。 */
double fdim(double first, double second);
