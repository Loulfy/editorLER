//
// Created by loulfy on 28/03/2023.
//

#ifndef LER_RDR_H
#define LER_RDR_H

#include "ler_env.hpp"
#include "ler_cam.hpp"

namespace ler
{
    class Renderer
    {
    public:

        virtual ~Renderer() = default;
        virtual void init(LerDevicePtr& device, const RenderPass& renderPass) = 0;
        virtual void render(vk::CommandBuffer cmd, const BatchedMesh& batch, int id) = 0;
        void update(const SceneConstant& constant) { m_constant = constant; }

    protected:

            PipelinePtr m_pipeline;
            SceneConstant m_constant;
            constexpr static vk::DeviceSize offset = 0;
    };

    class BoxRenderer : public Renderer
    {
    public:

        void init(LerDevicePtr& device, const RenderPass& renderPass) override;
        void render(vk::CommandBuffer cmd, const BatchedMesh& batch, int id) override;
    };

    class MeshRenderer : public Renderer
    {
    public:

        void init(LerDevicePtr& device, const RenderPass& renderPass) override;
        void render(vk::CommandBuffer cmd, const BatchedMesh& batch, int id) override;
    };
}

#endif //LER_RDR_H
