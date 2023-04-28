//
// Created by loulfy on 11/03/2023.
//

#include "ler_env.hpp"
#include "ler_log.hpp"
#include "ler_sys.hpp"

namespace ler
{
    void mergeSceneBuffer(const LerDevicePtr& dev, BatchedMesh& batch, BufferPtr& dest, uint32_t firstMesh, const aiScene* aiScene, const std::function<bool(aiMesh*)>& predicate, const std::function<void*(aiMesh*)>& provider)
    {
        void* data = nullptr;
        vmaMapMemory(dev->getVulkanContext().allocator, static_cast<VmaAllocation>(batch.staging->allocation), &data);
        auto cursor = static_cast<std::byte*>(data);
        size_t offset = 0;
        for(size_t i = 0; i < aiScene->mNumMeshes; ++i)
        {
            auto* mesh = aiScene->mMeshes[i];
            if(predicate(mesh))
                std::memcpy(cursor + offset * sizeof(glm::vec3), provider(mesh), mesh->mNumVertices * sizeof(glm::vec3));
            offset+= mesh->mNumVertices;
        }
        vmaUnmapMemory(dev->getVulkanContext().allocator, static_cast<VmaAllocation>(batch.staging->allocation));
        vk::CommandBuffer cmd = dev->getCommandBuffer();
        size_t dstOffset = batch.meshes[firstMesh].firstVertex * sizeof(glm::vec3);
        vk::BufferCopy copyRegion(0, dstOffset, batch.vertexCount * sizeof(glm::vec3));
        cmd.copyBuffer(batch.staging->handle, dest->handle, copyRegion);
        dev->submitAndWait(cmd);
    }

    std::array<glm::vec3, 8> createBox(const MeshInfo& mesh)
    {
        glm::vec3 min = mesh.bMin;
        glm::vec3 max = mesh.bMax;
        std::array<glm::vec3, 8> pts = {
            glm::vec3(max.x, max.y, max.z),
            glm::vec3(max.x, max.y, min.z),
            glm::vec3(max.x, min.y, max.z),
            glm::vec3(max.x, min.y, min.z),
            glm::vec3(min.x, max.y, max.z),
            glm::vec3(min.x, max.y, min.z),
            glm::vec3(min.x, min.y, max.z),
            glm::vec3(min.x, min.y, min.z),
        };

        return pts;
    }

    void addLine(std::vector<glm::vec3>& lines, const glm::vec3& p1, const glm::vec3& p2)
    {
        lines.push_back(p1);
        lines.push_back(p2);
    }

    void addBox(std::vector<glm::vec3>& lines, const  std::array<glm::vec3, 8>& pts)
    {
        addLine(lines, pts[0], pts[1]);
        addLine(lines, pts[2], pts[3]);
        addLine(lines, pts[4], pts[5]);
        addLine(lines, pts[6], pts[7]);

        addLine(lines, pts[0], pts[2]);
        addLine(lines, pts[1], pts[3]);
        addLine(lines, pts[4], pts[6]);
        addLine(lines, pts[5], pts[7]);

        addLine(lines, pts[0], pts[4]);
        addLine(lines, pts[1], pts[5]);
        addLine(lines, pts[2], pts[6]);
        addLine(lines, pts[3], pts[7]);
    }

    void BatchedMesh::allocate(const LerDevicePtr& device)
    {
        indexBuffer = device->createBuffer(8388608, vk::BufferUsageFlagBits::eIndexBuffer);
        vertexBuffer = device->createBuffer(8388608, vk::BufferUsageFlagBits::eVertexBuffer);
        aabbBuffer = device->createBuffer(24*500*sizeof(glm::vec3), vk::BufferUsageFlagBits::eVertexBuffer);
        staging = device->createBuffer(8388608, vk::BufferUsageFlags(), true);
    }

    bool BatchedMesh::appendMeshFromFile(const LerDevicePtr& device, const fs::path& path)
    {
        Assimp::Importer importer;
        fs::path cleanPath = path;
        log::info("Load scene: {}", cleanPath.make_preferred().string());
        unsigned int postProcess = aiProcessPreset_TargetRealtime_Fast;
        postProcess |= aiProcess_ConvertToLeftHanded;
        postProcess |= aiProcess_GenBoundingBoxes;
        // TODO: finish scene import
        //const aiScene* aiScene = importer.ReadFile(path.string(), postProcess);
        const auto blob = FileSystemService::Get().readFile(path);
        const aiScene* aiScene = importer.ReadFileFromMemory(blob.data(), blob.size(), postProcess, path.string().c_str());
        if(aiScene == nullptr || aiScene->mNumMeshes < 0)
            return false;

        // Prepare indirect data
        MeshInfo ind;
        uint32_t firstMesh = meshes.size();
        uint32_t indicesCount = 0;
        meshes.reserve(firstMesh+aiScene->mNumMeshes);
        for(size_t i = 0; i < aiScene->mNumMeshes; ++i)
        {
            auto* mesh = aiScene->mMeshes[i];
            log::debug("Mesh: {}", mesh->mName.C_Str());
            ind.countIndex = mesh->mNumFaces * 3;
            ind.firstIndex = indexCount;
            ind.countVertex = mesh->mNumVertices;
            ind.firstVertex = static_cast<int32_t>(vertexCount);
            ind.bMin = glm::make_vec3(&mesh->mAABB.mMin[0]);
            ind.bMax = glm::make_vec3(&mesh->mAABB.mMax[0]);
            ind.name = mesh->mName.C_Str();
            meshes.push_back(ind);

            indexCount+= ind.countIndex;
            vertexCount+= ind.countVertex;
            indicesCount+= ind.countIndex;
        }

        // Merge vertex
        mergeSceneBuffer(device, *this, vertexBuffer, firstMesh, aiScene, [](aiMesh* mesh){ return mesh->HasPositions(); }, [](aiMesh* mesh){ return mesh->mVertices; });

        // Merge index
        std::vector<uint32_t> indices;
        indices.reserve(indicesCount);
        for(size_t i = 0; i < aiScene->mNumMeshes; ++i)
        {
            auto *mesh = aiScene->mMeshes[i];
            if (mesh->HasFaces())
            {
                for (size_t j = 0; j < mesh->mNumFaces; ++j)
                {
                    indices.insert(indices.end(), mesh->mFaces[j].mIndices, mesh->mFaces[j].mIndices + 3);
                }
            }
        }

        size_t byteSize = indices.size() * sizeof(uint32_t);
        size_t dstOffset = meshes[firstMesh].firstIndex * sizeof(uint32_t);
        device->uploadBuffer(staging, indices.data(), byteSize);
        device->copyBuffer(staging, indexBuffer, byteSize, dstOffset);

        std::vector<glm::vec3> lines;
        lines.reserve(24*aiScene->mNumMeshes);
        for(size_t i = firstMesh; i < meshes.size(); ++i)
        {
            const auto& mesh = meshes[i];
            auto pts = createBox(mesh);
            addBox(lines, pts);
        }

        byteSize = lines.size()*sizeof(glm::vec3);
        dstOffset = firstMesh * 24 * sizeof(glm::vec3);
        device->uploadBuffer(staging, lines.data(), byteSize);
        device->copyBuffer(staging, aabbBuffer, byteSize, dstOffset);

        return true;
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
            log::debug("Mesh: {}", mesh->mName.C_Str());
            ind.countIndex = mesh->mNumFaces * 3;
            ind.firstIndex = batch.indexCount;
            ind.countVertex = mesh->mNumVertices;
            ind.firstVertex = static_cast<int32_t>(batch.vertexCount);
            ind.bMin = glm::make_vec3(&mesh->mAABB.mMin[0]);
            ind.bMax = glm::make_vec3(&mesh->mAABB.mMax[0]);
            ind.name = mesh->mName.C_Str();
            batch.meshes.push_back(ind);

            batch.indexCount+= ind.countIndex;
            batch.vertexCount+= ind.countVertex;
        }

        // Merge vertex
        mergeSceneBuffer(device, batch, batch.vertexBuffer, 0, aiScene, [](aiMesh* mesh){ return mesh->HasPositions(); }, [](aiMesh* mesh){ return mesh->mVertices; });

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

        std::vector<glm::vec3> lines;
        lines.reserve(5000);
        for(const auto & mesh : batch.meshes)
        {
            auto pts = createBox(mesh);
            addBox(lines, pts);
        }

        byteSize = 10*24*sizeof(glm::vec3);
        batch.aabbBuffer = device->createBuffer(byteSize, vk::BufferUsageFlagBits::eVertexBuffer);
        device->uploadBuffer(batch.staging, lines.data(), byteSize);
        device->copyBuffer(batch.staging, batch.aabbBuffer, byteSize);

        return batch;
    }
}