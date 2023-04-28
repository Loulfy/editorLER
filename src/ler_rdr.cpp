//
// Created by loulfy on 28/03/2023.
//

#include "ler_rdr.hpp"
#include "ler_sys.hpp"
#include "ler_log.hpp"

namespace ler
{
    void BoxRenderer::init(LerDevicePtr& device, const RenderPass& renderPass)
    {
        log::debug("Create Box renderer");
        ler::PipelineInfo info;
        info.topology = vk::PrimitiveTopology::eLineList;
        info.polygonMode = vk::PolygonMode::eLine;
        info.lineWidth = 2.f;

        std::vector<ler::ShaderPtr> shaders;
        shaders.push_back(device->createShader("aabb.vert.spv"));
        shaders.push_back(device->createShader("aabb.frag.spv"));
        m_pipeline = device->createGraphicsPipeline(renderPass, shaders, info);
    }

    void BoxRenderer::render(vk::CommandBuffer cmd, const BatchedMesh& batch, int id)
    {
        cmd.bindPipeline(m_pipeline->bindPoint, m_pipeline->handle.get());
        cmd.bindVertexBuffers(0, 1, &batch.aabbBuffer->handle, &offset);
        cmd.pushConstants(m_pipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(ler::SceneConstant), &m_constant);
        cmd.draw(24, 1, id*24, 0);
    }

    void MeshRenderer::init(LerDevicePtr &device, const RenderPass &renderPass)
    {
        log::debug("Create Mesh renderer");
        ler::PipelineInfo info;
        info.topology = vk::PrimitiveTopology::eTriangleList;
        info.polygonMode = vk::PolygonMode::eLine;

        std::vector<ler::ShaderPtr> shaders;
        shaders.emplace_back(device->createShader("mesh.vert.spv"));
        shaders.emplace_back(device->createShader("mesh.frag.spv"));
        m_pipeline = device->createGraphicsPipeline(renderPass, shaders, info);
    }

    void MeshRenderer::render(vk::CommandBuffer cmd, const BatchedMesh& batch, int id)
    {
        auto const& mesh = batch.meshes[id];
        cmd.bindPipeline(m_pipeline->bindPoint, m_pipeline->handle.get());
        cmd.pushConstants(m_pipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(ler::SceneConstant), &m_constant);
        cmd.bindIndexBuffer(batch.indexBuffer->handle, offset, vk::IndexType::eUint32);
        cmd.bindVertexBuffers(0, 1, &batch.vertexBuffer->handle, &offset);
        cmd.drawIndexed(mesh.countIndex, 1, mesh.firstIndex, mesh.firstVertex, 0);
    }
}