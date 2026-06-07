//
// Created by jehor on 2026/6/7.
//

#include "qt_media_helper.h"

#include "async_logger.h"
#include "os/windows/windows_sound.h"

namespace egret
{
    QtMediaHelper& QtMediaHelper::instance()
    {
        static QtMediaHelper qtm;
        return qtm;
    }

    void QtMediaHelper::playSound(std::uint32_t type)
    {
        const auto& t = magic_enum::enum_cast<SoundType>(type);
        if (t.has_value()) {
            playSystemSound(t.value());
        }
        else {
            LOG_WARN(std::format("Cannot Parse the Type Value: {}", std::to_string(type)));
        }
    }

    void QtMediaHelper::playSound(const QString& soundType)
    {
        const auto& t = magic_enum::enum_cast<SoundType>(soundType.toStdString());
        if (t.has_value()) {
            playSystemSound(t.value());
        }else {
            LOG_WARN(std::format("Cannot Parse the Type String: {}", soundType.toStdString()));
        }
    }
} // egret