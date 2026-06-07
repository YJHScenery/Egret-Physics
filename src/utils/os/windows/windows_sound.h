//
// Created by jehor on 2026/6/7.
//

#ifndef EGRET_PHYSICS_WINDOWS_SOUND_H
#define EGRET_PHYSICS_WINDOWS_SOUND_H
#include <cstdint>
#include <magic_enum.hpp>

namespace egret
{
    enum class SoundType: std::uint32_t
    {
        Ok,
        Question,
        Information,
        Warning,
        Error,
        Stop
    };

}

// ReSharper disable once CppRedundantNamespaceDefinition
namespace magic_enum::customize
{
    template <>
    struct enum_range<egret::SoundType>
    {
        static constexpr int min = 0;
        static constexpr int max = 255;
    };
}

namespace egret
{
    void playSystemSound(SoundType sound);

}

#endif //EGRET_PHYSICS_WINDOWS_SOUND_H
