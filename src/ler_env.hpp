//
// Created by loulfy on 11/03/2023.
//

#ifndef LER_ENV_H
#define LER_ENV_H

#include "common.hpp"
#include "ler_dev.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

namespace ler
{
    struct MeshInfo
    {
        uint32_t countIndex = 0;
        uint32_t firstIndex = 0;
        uint32_t countVertex = 0;
        int32_t firstVertex = 0;
        glm::vec3 bMin = glm::vec3(0.f);
        glm::vec3 bMax = glm::vec3(0.f);
        std::string name;
    };

    struct BatchedMesh
    {
        BufferPtr indexBuffer;
        BufferPtr vertexBuffer;
        BufferPtr aabbBuffer;
        BufferPtr staging;
        std::vector<MeshInfo> meshes;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;

        void allocate(const LerDevicePtr& device);
        bool appendMeshFromFile(const LerDevicePtr& device, const fs::path& path);
    };

    struct SceneConstant
    {
        glm::mat4 proj = glm::mat4(1.f);
        glm::mat4 view = glm::mat4(1.f);
    };

    BatchedMesh loadMeshFromFile(const LerDevicePtr& device, const fs::path& path);
}

#endif //LER_ENV_H
