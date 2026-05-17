//
// Created by GitHub Copilot on 2026/5/10.
//

#include "shape_factory_registry.h"

#include "shape_box.h"
#include "shape_cylinder.h"
#include "shape_sphere.h"
#include "world_scene_manager.h"

namespace egret
{
    namespace
    {
        bool registerStandardShapeFactories()
        {
            auto &registry = ShapeFactoryRegistry::instance();

            registry.registerFactory(TYPE_ID_STANDARD_SPHERE, [](const ShapeLoadInfo &info) -> std::unique_ptr<ShapeBase>
                                     {
                const auto it = info.parameters.find("radius");
                if (it == info.parameters.end()) {
                    return nullptr;
                }

                const auto radius = std::get_if<double>(&it->second);
                if (radius == nullptr) {
                    return nullptr;
                }

                return std::make_unique<ShapeSphere>(*radius); });

            registry.registerFactory(TYPE_ID_STANDARD_BOX, [](const ShapeLoadInfo &info) -> std::unique_ptr<ShapeBase>
                                     {
                const auto it = info.parameters.find("size");
                if (it == info.parameters.end()) {
                    return nullptr;
                }

                const auto size = std::get_if<Eigen::Vector3d>(&it->second);
                if (size == nullptr) {
                    return nullptr;
                }

                return std::make_unique<ShapeBox>(*size); });

            registry.registerFactory(TYPE_ID_STANDARD_CYLINDER, [](const ShapeLoadInfo &info) -> std::unique_ptr<ShapeBase>
                                     {
                const auto radiusIt = info.parameters.find("radius");
                const auto heightIt = info.parameters.find("height");
                if (radiusIt == info.parameters.end() || heightIt == info.parameters.end()) {
                    return nullptr;
                }

                const auto radius = std::get_if<double>(&radiusIt->second);
                const auto height = std::get_if<double>(&heightIt->second);
                if (radius == nullptr || height == nullptr) {
                    return nullptr;
                }

                return std::make_unique<ShapeCylinder>(*radius, *height); });

            return true;
        }

        const bool g_standardShapeFactoriesRegistered = registerStandardShapeFactories();
    }
}
