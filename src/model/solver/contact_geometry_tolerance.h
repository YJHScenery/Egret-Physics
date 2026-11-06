/** @file contact_geometry_tolerance.h @brief 根据物体尺寸限制接触容差，避免薄物体的虚假接触和过度穿透。 */
#pragma once
#include "physical_entity.h"
#include "shape_box.h"
#include <algorithm>

namespace egret
{
    /** @brief 返回正的特征厚度；盒体采用缩放后的局部最短边，避免旋转放大厚度。 */
    inline double contactFeatureLength(const PhysicalEntity& body)
    {
        const auto shape = body.getShape();
        if (!shape) return 1.0;
        if (const auto* box = dynamic_cast<const ShapeBox*>(shape.get()))
            return std::max(1e-12, box->getSize().cwiseProduct(body.getTransformCR().getScale().cwiseAbs()).minCoeff());
        const auto bounds = shape->getAABB(body.getTransformCR());
        return std::max(1e-12, (bounds.max-bounds.min).minCoeff());
    }

    /** @brief 保留用户容差上限，同时保证容差远小于双方物体的特征厚度。 */
    inline double scaledContactTolerance(const PhysicalEntity& a, const PhysicalEntity& b,
                                         double ceiling, double ratio)
    {
        return std::min(ceiling, ratio * std::min(contactFeatureLength(a), contactFeatureLength(b)));
    }
}
