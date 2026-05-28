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
            auto& registry = ShapeFactoryRegistry::instance();

            registry.registerFactory(static_cast<std::uint32_t>(ShapeID::Sphere),
                                     [](const ShapeLoadInfo& info) -> std::unique_ptr<ShapeBase>
                                     {
                                         const auto it = info.numberParams.find("radius");
                                         if (it == info.numberParams.end()) {
                                             return nullptr;
                                         }

                                         const auto radius = &it->second[0];
                                         if (radius == nullptr) {
                                             return nullptr;
                                         }

                                         return std::make_unique<ShapeSphere>(*radius);
                                     });

            registry.registerFactory(static_cast<std::uint32_t>(ShapeID::Box), [](const ShapeLoadInfo& info) -> std::unique_ptr<ShapeBase>
            {
                const auto it = info.numberParams.find("size");
                if (it == info.numberParams.end()) {
                    return nullptr;
                }
                Eigen::Vector3d size;
                if (it->second.size() >= 3) {
                    size = Eigen::Vector3d(it->second.data());
                }

                return std::make_unique<ShapeBox>(size);
            });

            registry.registerFactory(static_cast<std::uint32_t>(ShapeID::Cylinder),
                                     [](const ShapeLoadInfo& info) -> std::unique_ptr<ShapeBase>
                                     {
                                         const auto radiusIt = info.numberParams.find("radius");
                                         const auto heightIt = info.numberParams.find("height");
                                         if (radiusIt == info.numberParams.end() || heightIt == info.numberParams.
                                             end()) {
                                             return nullptr;
                                         }

                                         if (radiusIt->second.size() >= 1 && heightIt->second.size() >= 1) {
                                             const auto radius = radiusIt->second[0];
                                             const auto height = heightIt->second[0];


                                             return std::make_unique<ShapeCylinder>(radius, height);
                                         }
                                         return nullptr;
                                     });

            return true;
        }

        const bool g_standardShapeFactoriesRegistered = registerStandardShapeFactories();
    }
}
