#include <iostream>
#include "ler.hpp"

int main()
{
    std::cout << ler::getHomeDir() << std::endl;

    ler::GlslangInitializer initme;
    ler::shaderAutoCompile();

    ler::LerApp app;
    auto dev = app.getDevice();
    std::vector<ler::ShaderPtr> shaders;
    shaders.emplace_back(dev->createShader(ler::ASSETS_DIR / "quad.vert.spv"));
    shaders.emplace_back(dev->createShader(ler::ASSETS_DIR / "quad.frag.spv"));
    auto renderPass = dev->createSimpleRenderPass(vk::Format::eR8G8B8A8Unorm);

    ler::PipelineInfo info;
    info.textureCount = 0;
    info.writeDepth = true;
    info.subPass = 0;
    info.topology = vk::PrimitiveTopology::eTriangleStrip;
    info.polygonMode = vk::PolygonMode::eFill;
    info.sampleCount = vk::SampleCountFlagBits::e1;
    auto pipeline = dev->createGraphicsPipeline(renderPass, shaders, info);

    vk::Rect2D renderArea(vk::Offset2D(), vk::Extent2D(720, 480));
    auto frameBuffer = dev->createFrameBuffer(renderPass, vk::Extent2D(720, 480));
    auto clearValues = ler::LerDevice::clearRenderPass(renderPass);
    vk::Viewport viewport(0, 0, 720.f, 480.f, 0, 1.0f);

    auto cmd = dev->getCommandBuffer();
    vk::RenderPassBeginInfo beginInfo;
    beginInfo.setRenderPass(renderPass.handle.get());
    beginInfo.setFramebuffer(frameBuffer.handle.get());
    beginInfo.setRenderArea(renderArea);
    beginInfo.setClearValues(clearValues);
    cmd.beginRenderPass(beginInfo, vk::SubpassContents::eInline);
    cmd.bindPipeline(pipeline->bindPoint, pipeline->handle.get());
    cmd.setScissor(0, 1, &renderArea);
    cmd.setViewport(0, 1, &viewport);
    cmd.draw(8, 1, 0, 0);
    cmd.endRenderPass();
    dev->submitAndWait(cmd);

    //auto texture = dev->loadTextureFromFile(ler::ASSETS_DIR / "minimalLER.png");
    auto texture = frameBuffer.images.front();
    auto sampler = dev->createSampler(vk::SamplerAddressMode::eClampToEdge, true);
    auto ds = ImGui_ImplVulkan_AddTexture(static_cast<VkSampler>(sampler.get()), static_cast<VkImageView>(texture->view.get()), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    app.show([ds](){
        bool open = true;
        ImGui::Begin("Hello Gui", &open, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Image((ImTextureID)ds, ImVec2(720, 480));
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
    });
    app.run();

    return EXIT_SUCCESS;
}
