//
// Created by loulfy on 24/02/2023.
//

#ifndef LER_SYS_H
#define LER_SYS_H

#include "ler.hpp"

#ifdef _WIN32
    #include <Windows.h>
    #include <ShlObj.h>
#else
    #include <unistd.h>
    #include <pwd.h>
#endif

namespace ler
{
    std::string getHomeDir()
    {
        #ifdef _WIN32
        wchar_t wide_path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, SHGFP_TYPE_CURRENT, wide_path))) {
            char path[MAX_PATH];
            if (WideCharToMultiByte(CP_UTF8, 0, wide_path, -1, path, MAX_PATH, nullptr, nullptr) > 0) {
                return path;
            }
        }
        #else
        struct passwd* pw = getpwuid(getuid());
        if (pw != nullptr)
            return pw->pw_dir;
        #endif

        return {};
    }
}

#endif //LER_SYS_H
