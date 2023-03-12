//
// Created by loulfy on 08/03/2023.
//

#include "ler_spv.hpp"
#include "ler_sys.hpp"
#include "ler_log.hpp"

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <SPIRV/doc.h>

namespace ler
{
    struct KindMapping
    {
        fs::path ext;
        EShLanguage kind;
    };

    static const std::array<KindMapping, 4> c_KindMap = {{
        {".vert", EShLanguage::EShLangVertex},
        {".frag", EShLanguage::EShLangFragment},
        {".geom", EShLanguage::EShLangGeometry},
        {".comp", EShLanguage::EShLangCompute}
    }};

    std::optional<KindMapping> convertShaderExtension(const fs::path& ext)
    {
        for(auto& map : c_KindMap)
            if(map.ext == ext)
                return map;
        return {};
    }

    GlslangInitializer::GlslangInitializer()
    {
        glslang::InitializeProcess();
    }

    GlslangInitializer::~GlslangInitializer()
    {
        glslang::FinalizeProcess();
    }

    bool compile(glslang::TShader* shader, const std::string& code, EShMessages controls, const std::string& shaderName, const std::string& entryPointName = "main")
    {
        const char* shaderStrings = code.data();
        const int shaderLengths = static_cast<int>(code.size());
        const char* shaderNames = nullptr;

        if (controls & EShMsgDebugInfo)
        {
            shaderNames = shaderName.data();
            shader->setStringsWithLengthsAndNames(&shaderStrings, &shaderLengths, &shaderNames, 1);
        }
        else
        {
            shader->setStringsWithLengths(&shaderStrings, &shaderLengths, 1);
        }

        if (!entryPointName.empty())
            shader->setEntryPoint(entryPointName.c_str());

        return shader->parse(GetDefaultResources(), 460, false, controls);
    }

    std::vector<uint32_t> compileGlslToSpv(const std::string& code, const fs::path& name)
    {
        auto stage = convertShaderExtension(name.extension());
        if(!stage.has_value())
            return {};

        bool success = true;
        glslang::TShader shader(stage.value().kind);
        EShMessages controls = EShMsgCascadingErrors;
        controls = static_cast<EShMessages>(controls | EShMsgDebugInfo);
        controls = static_cast<EShMessages>(controls | EShMsgSpvRules);
        controls = static_cast<EShMessages>(controls | EShMsgKeepUncalled);
        controls = static_cast<EShMessages>(controls | EShMsgVulkanRules | EShMsgSpvRules);
        success &= compile(&shader, code, controls, name.string());

        if(!success)
        {
            log::error(shader.getInfoLog());
            return {};
        }

        // Link all of them.
        glslang::TProgram program;
        program.addShader(&shader);
        success &= program.link(controls);

        if(!success)
        {
            log::error(program.getInfoLog());
            return {};
        }

        glslang::SpvOptions options;
        spv::SpvBuildLogger logger;
        std::vector<uint32_t> spv;
        options.disableOptimizer = false;
        options.optimizeSize = true;
        glslang::GlslangToSpv(*program.getIntermediate(shader.getStage()), spv, &logger, &options);
        if(!logger.getAllMessages().empty())
            log::error(logger.getAllMessages());
        return spv;
    }

    void compileFile(const fs::path& input, const fs::path& output)
    {
        std::ifstream fin(input);
        std::stringstream src;
        src << fin.rdbuf();
        fin.close();

        auto spv = compileGlslToSpv(src.str(), input.filename().string());

        if(spv.empty())
            return;

        std::ofstream fout(output, std::ios_base::binary);
        auto size = static_cast<std::streamsize>(spv.size() * sizeof(uint32_t));
        fout.write(reinterpret_cast<const char*>(spv.data()), size);
        fout.close();
    }

    void shaderAutoCompile()
    {
        for (const auto& entry : fs::directory_iterator(ASSETS_DIR))
        {
            auto res = convertShaderExtension(entry.path().extension());
            if(!res.has_value())
                continue;

            fs::path f = entry.path();
            f.concat(".spv");
            if(fs::exists(f) && fs::last_write_time(f) > entry.last_write_time())
                continue;

            log::warn("Compile {}", entry.path().string());
            compileFile(entry.path(), f);
        }
    }
}