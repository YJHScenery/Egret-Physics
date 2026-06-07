//
// Created by jehor on 2026/6/7.
//
#include "windows_sound.h"

#include <tuple>

#if _WIN32

#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

namespace egret
{
    void playSystemSound(const SoundType sound)
    {
        switch (sound) {
        case SoundType::Ok: {
            MessageBeep(MB_OK);
            break;
        }
        case SoundType::Question: {
            MessageBeep(MB_ICONQUESTION);
            break;
        }
        case SoundType::Information: {
            MessageBeep(MB_ICONINFORMATION);
            break;
        }
        case SoundType::Warning: {
            MessageBeep(MB_ICONWARNING);
            break;
        }
        case SoundType::Error: {
            MessageBeep(MB_ICONERROR);
            break;
        }
        case SoundType::Stop: {
            MessageBeep(MB_ICONSTOP);
            break;
        }
        default: {
            MessageBeep(MB_ICONERROR);
            break;
        }
        }
    }
#else
void playSystemSound(SoundType sound)
    {
        std::ignore = sound;
        //TODO: 需要实现跨平台逻辑
    }



#endif
}