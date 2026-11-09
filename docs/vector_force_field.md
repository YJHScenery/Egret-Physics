/** @file vector_force_field.md @brief 矢量力场与可视化使用说明。 */

# 矢量力场

在场景编辑器中选择“场 → 可编程矢量力场”。源码定义以下两个受限 C 函数：

```c
bool field_contains(Vector3 p) {
    return field_inside_sphere(p, vector3(0, 0, 0), 5);
}
Vector3 field_force(double t, Vector3 p) {
    return vector3(0, 0, -10 * math_cos(t));
}
```

位置单位为米，时间单位为秒，返回的矢量是牛顿单位的力，直接作用于作用域内的活动物体，不乘以质量。`field_inside_box(p, minimum, maximum)` 和 `field_inside_sphere(p, center, radius)` 包含边界。`math_cos` 接受正负一百万弧度以内的输入。场函数不能使用全局变量、静态变量或场景控制 API；可使用 `vector3`、`math_sqrt`、`math_abs` 及上述辅助函数。每次采样受 Wasm 燃料预算限制。


C++ 可通过 `VectorForceModel` 自定义作用域和随时间变化的力函数，再构造 `VectorForceField`。非保守场的通用势能不可确定，目前 `potential` 返回零，不能用它计算此类场的势能。

