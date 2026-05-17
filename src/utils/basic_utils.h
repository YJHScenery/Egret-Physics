//
// Created by jehor on 2026/4/24.
//

#ifndef EGRET_PHYSICS_GEOMETRY_H
#define EGRET_PHYSICS_GEOMETRY_H


#define EGRET_PHYSICS_VERSION_MAJOR 0
#define EGRET_PHYSICS_VERSION_MINOR 0
#define EGRET_PHYSICS_VERSION_PATCH 1

#define EGRET_PHYSICS_VERSION_STRING "0.0.1"

#define EGRET_PHYSICS_APPLICATION_DISPLAY_NAME "Egret Physics Studio"

#define EGRET_PHYSICS_APPLICATION_NAME "EgretPhysicsStudio"


#include <type_traits>
#include <concepts>
#include <type_traits>


namespace egret
{
    template <typename Base, std::integral Exponent, Exponent Ex>
    constexpr Base quickPower(Base base)
    {
        if constexpr (Ex == 0) {
            return Base(1);
        } else if constexpr (Ex < 0) {
            return Base(1) / quickPower<Base, Exponent, -Ex>(base);
        } else if constexpr (Ex == 1) {
            return base;
        } else if constexpr (Ex % 2 == 0) {
            auto half = quickPower<Base, Exponent, Ex / 2>(base);
            return half * half;
        } else {
            return base * quickPower<Base, Exponent, Ex - 1>(base);
        }
    }

    constexpr auto quickSquare = []<typename Base>(Base x) {
        return quickPower<Base, int, 2>(x);
    };
}
#endif //EGRET_PHYSICS_GEOMETRY_H
