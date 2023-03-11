//
// Created by loulfy on 09/03/2023.
//

#ifndef LER_SYS_H
#define LER_SYS_H

#include "common.hpp"

namespace ler
{
    static const fs::path ASSETS_DIR = fs::path(PROJECT_DIR) / "assets";
    std::string getHomeDir();
}

#endif //LER_SYS_H
