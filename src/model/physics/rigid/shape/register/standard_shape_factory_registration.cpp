//
// Created by GitHub Copilot on 2026/5/10.
//

#include "shape_factory_registry.h"

#include "shape_box.h"
#include "shape_cylinder.h"
#include "shape_sphere.h"
#include "shape_spherical_shell.h"
#include "shape_cylindrical_shell.h"
#include "shape_disk.h"
#include "shape_ring.h"
#include "shape_rod.h"
#include "world_scene_manager.h"

namespace egret
{
    namespace
    {
        bool registerStandardShapeFactories()
        {
            auto& registry = ShapeFactoryRegistry::instance();

            registry.registerFactory(static_cast<std::uint32_t>(ShapeType::Sphere),
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

            registry.registerFactory(static_cast<std::uint32_t>(ShapeType::Box),
                                     [](const ShapeLoadInfo& info) -> std::unique_ptr<ShapeBase>
                                     {
                                         const auto it = info.numberParams.find("size");
                                         if (it == info.numberParams.end()) {
                                             return nullptr;
                                         }
                                         Eigen::Vector3d size;
                                         if (it->second.size() >= 3) {
                                             size = Eigen::Vector3d(it->second[0], it->second[1], it->second[2]);
                                         }

                                         return std::make_unique<ShapeBox>(size);
                                     });

            registry.registerFactory(static_cast<std::uint32_t>(ShapeType::Cylinder),
                                     [](const ShapeLoadInfo& info) -> std::unique_ptr<ShapeBase>
                                     {
                                         const auto radiusIt = info.numberParams.find("radius");
                                         const auto heightIt = info.numberParams.find("height");
                                         if (radiusIt == info.numberParams.end() || heightIt == info.numberParams.
                                             end()) {
                                             return nullptr;
                                         }

                                         if (!radiusIt->second.empty() && !heightIt->second.empty()) {
                                             const auto radius = radiusIt->second[0];
                                             const auto height = heightIt->second[0];
                                             return std::make_unique<ShapeCylinder>(radius, height);
                                         }
                                         return nullptr;
                                     });

            registry.registerFactory(static_cast<std::uint32_t>(ShapeType::CylindricalShell),
                                     [](const ShapeLoadInfo& info) -> std::unique_ptr<ShapeBase>
                                     {
                                         const auto radiusIt = info.numberParams.find("radius");
                                         const auto heightIt = info.numberParams.find("height");
                                         if (radiusIt == info.numberParams.end() || heightIt == info.numberParams.
                                             end()) {
                                             return nullptr;
                                         }

                                         if (!radiusIt->second.empty() && !heightIt->second.empty()) {
                                             const auto radius = radiusIt->second[0];
                                             const auto height = heightIt->second[0];

                                             return std::make_unique<ShapeCylindricalShell>(radius, height);
                                         }
                                         return nullptr;
                                     });

            registry.registerFactory(static_cast<std::uint32_t>(ShapeType::Disk),
                                     [](const ShapeLoadInfo& info) -> std::unique_ptr<ShapeBase>
                                     {
                                         const auto radiusIt = info.numberParams.find("radius");
                                         if (radiusIt == info.numberParams.end()) {
                                             return nullptr;
                                         }

                                         if (!radiusIt->second.empty()) {
                                             const auto radius = radiusIt->second[0];
                                             return std::make_unique<ShapeDisk>(radius);
                                         }
                                         return nullptr;
                                     });

            registry.registerFactory(static_cast<std::uint32_t>(ShapeType::Ring),
                                     [](const ShapeLoadInfo& info) -> std::unique_ptr<ShapeBase>
                                     {
                                         const auto radiusIt = info.numberParams.find("radius");
                                         if (radiusIt == info.numberParams.end()) {
                                             return nullptr;
                                         }

                                         if (!radiusIt->second.empty()) {
                                             const auto radius = radiusIt->second[0];
                                             return std::make_unique<ShapeRing>(radius);
                                         }
                                         return nullptr;
                                     });

            registry.registerFactory(static_cast<std::uint32_t>(ShapeType::Rod),
                                     [](const ShapeLoadInfo& info) -> std::unique_ptr<ShapeBase>
                                     {
                                         const auto lengthIt = info.numberParams.find("length");
                                         if (lengthIt == info.numberParams.end()) {
                                             return nullptr;
                                         }

                                         if (!lengthIt->second.empty()) {
                                             const auto length = lengthIt->second[0];
                                             return std::make_unique<ShapeRod>(length);
                                         }
                                         return nullptr;
                                     });

            registry.registerFactory(static_cast<std::uint32_t>(ShapeType::SphericalShell),
                                     [](const ShapeLoadInfo& info) -> std::unique_ptr<ShapeBase>
                                     {
                                         const auto radiusIt = info.numberParams.find("radius");
                                         if (radiusIt == info.numberParams.end()) {
                                             return nullptr;
                                         }

                                         if (!radiusIt->second.empty()) {
                                             const auto radius = radiusIt->second[0];
                                             return std::make_unique<ShapeSphericalShell>(radius);
                                         }
                                         return nullptr;
                                     });

            return true;
        }

        const bool g_standardShapeFactoriesRegistered = registerStandardShapeFactories();
    }
}
