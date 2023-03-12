//
// Created by loulfy on 11/03/2023.
//

#include "ler_env.hpp"
#include "ler_log.hpp"

namespace ler
{
    void mergeSceneBuffer(const LerDevicePtr& dev, BatchedMesh& scene, BufferPtr& dest, const aiScene* aiScene, const std::function<bool(aiMesh*)>& predicate, const std::function<void*(aiMesh*)>& provider)
    {
        void* data = nullptr;
        vmaMapMemory(dev->getVulkanContext().allocator, static_cast<VmaAllocation>(scene.staging->allocation), &data);
        auto cursor = static_cast<std::byte*>(data);
        for(size_t i = 0; i < aiScene->mNumMeshes; ++i)
        {
            auto* mesh = aiScene->mMeshes[i];
            if(predicate(mesh))
                std::memcpy(cursor + scene.meshes[i].firstVertex * sizeof(glm::vec3), provider(mesh), scene.meshes[i].countVertex * sizeof(glm::vec3));
        }
        vmaUnmapMemory(dev->getVulkanContext().allocator, static_cast<VmaAllocation>(scene.staging->allocation));
        vk::CommandBuffer cmd = dev->getCommandBuffer();
        vk::BufferCopy copyRegion(0, 0, scene.vertexCount * sizeof(glm::vec3));
        cmd.copyBuffer(scene.staging->handle, dest->handle, copyRegion);
        dev->submitAndWait(cmd);
    }

    BatchedMesh loadMeshFromFile(const LerDevicePtr& device, const fs::path& path)
    {
        Assimp::Importer importer;
        fs::path cleanPath = path;
        log::info("Load scene: {}", cleanPath.make_preferred().string());
        unsigned int postProcess = aiProcessPreset_TargetRealtime_Fast;
        postProcess |= aiProcess_ConvertToLeftHanded;
        postProcess |= aiProcess_GenBoundingBoxes;
        const aiScene* aiScene = importer.ReadFile(path.string(), postProcess);
        if(aiScene == nullptr || aiScene->mNumMeshes < 0)
            return {};

        BatchedMesh batch;
        batch.indexBuffer = device->createBuffer(8388608, vk::BufferUsageFlagBits::eIndexBuffer);
        batch.vertexBuffer = device->createBuffer(8388608, vk::BufferUsageFlagBits::eVertexBuffer);
        batch.staging = device->createBuffer(8388608, vk::BufferUsageFlags(), true);

        MeshInfo ind;

        // Prepare indirect data
        batch.meshes.reserve(aiScene->mNumMeshes);
        for(size_t i = 0; i < aiScene->mNumMeshes; ++i)
        {
            auto* mesh = aiScene->mMeshes[i];
            ind.countIndex = mesh->mNumFaces * 3;
            ind.firstIndex = batch.indexCount;
            ind.countVertex = mesh->mNumVertices;
            ind.firstVertex = batch.vertexCount;
            ind.bMin = glm::make_vec3(&mesh->mAABB.mMin[0]);
            ind.bMax = glm::make_vec3(&mesh->mAABB.mMax[0]);
            batch.meshes.push_back(ind);

            batch.indexCount+= ind.countIndex;
            batch.vertexCount+= ind.countVertex;
        }

        // Merge vertex
        mergeSceneBuffer(device, batch, batch.vertexBuffer, aiScene, [](aiMesh* mesh){ return mesh->HasPositions(); }, [](aiMesh* mesh){ return mesh->mVertices; });

        // Merge index
        std::vector<uint32_t> indices;
        indices.reserve(batch.indexCount);
        for(size_t i = 0; i < aiScene->mNumMeshes; ++i)
        {
            auto *mesh = aiScene->mMeshes[i];
            if (mesh->HasFaces())
            {
                for (size_t j = 0; j < mesh->mNumFaces; j++)
                {
                    indices.insert(indices.end(), mesh->mFaces[j].mIndices, mesh->mFaces[j].mIndices + 3);
                }
            }
        }

        size_t byteSize = indices.size() * sizeof(uint32_t);
        device->uploadBuffer(batch.staging, indices.data(), byteSize);
        device->copyBuffer(batch.staging, batch.indexBuffer, byteSize);

        return batch;
    }
}