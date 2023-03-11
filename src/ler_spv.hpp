//
// Created by loulfy on 09/03/2023.
//

#ifndef LER_SPV_H
#define LER_SPV_H

#include "common.hpp"

namespace ler
{
    class GlslangInitializer
    {
    public:

        GlslangInitializer();
        ~GlslangInitializer();
    };

    std::vector<uint32_t> compileGlslToSpv(const std::string& code, const fs::path& name);
    void shaderAutoCompile();
}

#endif //LER_SPV_H
