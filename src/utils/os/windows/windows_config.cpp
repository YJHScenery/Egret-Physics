//
// Created by jehor on 2026/5/9.
//
#include "windows_config.h"

#if (__has_include(<Windows.h>))
#include <Windows.h>
void setWindowsTerminalUtf8()
{
    SetConsoleOutputCP(CP_UTF8);
}
#else
void setWindowsTerminalUtf8()
{
    return;
}
#endif

