#include <iostream>
#include "ler.hpp"

int main()
{
    ler::log::set_level(ler::log::level::level_enum::debug);
    ler::GlslangInitializer initme;
    ler::shaderAutoCompile();

    ler::LerApp app;
    auto dev = app.getDevice();
    std::vector<ler::ShaderPtr> shaders;
    shaders.emplace_back(dev->createShader(ler::ASSETS_DIR / "mesh.vert.spv"));
    shaders.emplace_back(dev->createShader(ler::ASSETS_DIR / "mesh.frag.spv"));
    auto renderPass = dev->createSimpleRenderPass(vk::Format::eR8G8B8A8Unorm);

    auto batch = ler::loadMeshFromFile(dev, ler::ASSETS_DIR / "Avocado.glb");
    ler::SceneConstant constant;
    constant.proj = glm::perspective(glm::radians(55.f), 1920.f / 1080.f, 0.01f, 10000.0f);
    auto& mesh = batch.meshes[0];
    glm::vec3 center = (mesh.bMax + mesh.bMin) * 0.5f;
    float vFov = glm::radians(55.f);
    float ratio = 2 * std::tan( vFov / 2 );
    //ratio-= 0.1;
    float size = mesh.bMax.y * 2;
    float Z = size / ratio;
    constant.view = glm::lookAt(glm::vec3(0.0f, 0.0f, Z), center, glm::vec3(0.0f, 1.0f, 0.0f));
    constant.proj[1][1] *= -1;

    ler::PipelineInfo info;
    info.textureCount = 0;
    info.writeDepth = true;
    info.subPass = 0;
    info.topology = vk::PrimitiveTopology::eTriangleList;
    info.polygonMode = vk::PolygonMode::eLine;
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
    //cmd.draw(8, 1, 0, 0);
    vk::DeviceSize offset = 0;
    cmd.pushConstants(pipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(ler::SceneConstant), &constant);
    cmd.bindIndexBuffer(batch.indexBuffer->handle, offset, vk::IndexType::eUint32);
    cmd.bindVertexBuffers(0, 1, &batch.vertexBuffer->handle, &offset);
    cmd.drawIndexed(mesh.countIndex, 1, mesh.firstIndex, mesh.firstVertex, 0);
    cmd.endRenderPass();
    dev->submitAndWait(cmd);

    //auto texture = dev->loadTextureFromFile(ler::ASSETS_DIR / "minimalLER.png");
    auto texture = frameBuffer.images.front();
    auto sampler = dev->createSampler(vk::SamplerAddressMode::eClampToEdge, true);
    auto ds = ImGui_ImplVulkan_AddTexture(static_cast<VkSampler>(sampler.get()), static_cast<VkImageView>(texture->view.get()), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    app.show([&](){
        bool open = true;
        ImGui::Begin("Hello Gui", &open, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::ImageButton((ImTextureID)ds, ImVec2(720, 480));
        bool isHovered = ImGui::IsItemHovered();
        bool isFocused = ImGui::IsItemFocused();
        ImVec2 mousePositionAbsolute = ImGui::GetMousePos();
        ImVec2 screenPositionAbsolute = ImGui::GetItemRectMin();
        ImVec2 mousePositionRelative = ImVec2(mousePositionAbsolute.x - screenPositionAbsolute.x, mousePositionAbsolute.y - screenPositionAbsolute.y);
        ImGui::Text("Is mouse over screen? %s", isHovered ? "Yes" : "No");
        ImGui::Text("Is screen focused? %s", isFocused ? "Yes" : "No");
        ImGui::Text("Position: %f, %f", mousePositionRelative.x, mousePositionRelative.y);
        ImGui::Text("Mouse clicked: %s", ImGui::IsMouseDown(ImGuiMouseButton_Left) ? "Yes" : "No");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();

        /*auto cmd = dev->getCommandBuffer();
            vk::RenderPassBeginInfo beginInfo;
            beginInfo.setRenderPass(renderPass.handle.get());
            beginInfo.setFramebuffer(frameBuffer.handle.get());
            beginInfo.setRenderArea(renderArea);
            beginInfo.setClearValues(clearValues);
            cmd.beginRenderPass(beginInfo, vk::SubpassContents::eInline);
            cmd.bindPipeline(pipeline->bindPoint, pipeline->handle.get());
            cmd.setScissor(0, 1, &renderArea);
            cmd.setViewport(0, 1, &viewport);
            //cmd.draw(8, 1, 0, 0);
            auto& mesh = batch.meshes.front();
            vk::DeviceSize offset = 0;
            cmd.pushConstants(pipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(ler::SceneConstant), &constant);
            cmd.bindIndexBuffer(batch.indexBuffer->handle, offset, vk::IndexType::eUint32);
            cmd.bindVertexBuffers(0, 1, &batch.vertexBuffer->handle, &offset);
            cmd.drawIndexed(mesh.countIndex, 1, mesh.firstIndex, mesh.firstVertex, 0);
            cmd.endRenderPass();
            dev->submitAndWait(cmd);*/
    });
    app.run();

    return EXIT_SUCCESS;
}
