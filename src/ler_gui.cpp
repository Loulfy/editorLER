//
// Created by loulfy on 06/03/2023.
//

#include "ler.hpp"

namespace ler
{
    vk::UniqueDescriptorPool ImguiImpl::createPool(LerDevicePtr& device)
    {
        auto ctx = device->getVulkanContext();
        std::array<vk::DescriptorPoolSize, 11> pool_sizes =
        {{
             {vk::DescriptorType::eSampler, 1000},
             {vk::DescriptorType::eCombinedImageSampler, 1000},
             {vk::DescriptorType::eSampledImage, 1000},
             {vk::DescriptorType::eStorageImage, 1000},
             {vk::DescriptorType::eUniformTexelBuffer, 1000},
             {vk::DescriptorType::eStorageTexelBuffer, 1000},
             {vk::DescriptorType::eUniformBuffer, 1000},
             {vk::DescriptorType::eStorageBuffer, 1000},
             {vk::DescriptorType::eUniformBufferDynamic, 1000},
             {vk::DescriptorType::eStorageBufferDynamic, 1000},
             {vk::DescriptorType::eInputAttachment, 1000}
         }};

        vk::DescriptorPoolCreateInfo poolInfo;
        poolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
        poolInfo.setPoolSizes(pool_sizes);
        poolInfo.setMaxSets(1000);

        return ctx.device.createDescriptorPoolUnique(poolInfo);
    }

    void ImguiImpl::init(LerDevicePtr& device, vk::DescriptorPool pool, RenderPass& renderPass, GLFWwindow* window)
    {
        auto ctx = device->getVulkanContext();

        int w, h;
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        glfwGetFramebufferSize(window, &w, &h);
        io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));
        io.ConfigFlags = ImGuiConfigFlags_DockingEnable;
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForVulkan(window, true);

        //this initializes imgui for Vulkan
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = ctx.instance;
        init_info.PhysicalDevice = ctx.physicalDevice;
        init_info.Device = ctx.device;
        init_info.Queue = ctx.device.getQueue(ctx.graphicsQueueFamily, 0);
        init_info.DescriptorPool = pool;
        init_info.MinImageCount = 2;
        init_info.ImageCount = 2;
        init_info.Subpass = 2;
        init_info.MSAASamples = VK_SAMPLE_COUNT_8_BIT;
        ImGui_ImplVulkan_Init(&init_info, renderPass.handle.get());

        auto cmd = device->getCommandBuffer();
        ImGui_ImplVulkan_CreateFontsTexture(cmd);
        device->submitAndWait(cmd);
    }

    void ImguiImpl::begin()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImguiImpl::end(vk::CommandBuffer cmd)
    {
        ImGui::Render();
        // Record dear imgui primitives into command buffer
        ImDrawData* draw_data = ImGui::GetDrawData();
        ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);
    }

    void ImguiImpl::clean()
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}