//
// Created by loulfy on 22/02/2023.
//

#ifndef LER_H
#define LER_H

#include "ler_log.hpp"
#include "ler_sys.hpp"
#include "ler_dev.hpp"
#include "ler_spv.hpp"
#include "ler_env.hpp"
#include "ler_arc.hpp"

#define GLFW_INCLUDE_NONE // Do not include any OpenGL/Vulkan headers
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>

namespace ler
{
    struct ImguiImpl
    {
        static vk::UniqueDescriptorPool createPool(LerDevicePtr& device);
        static void init(LerDevicePtr& device, vk::DescriptorPool pool, RenderPass& renderPass, GLFWwindow* window);
        static void begin();
        static void end(vk::CommandBuffer cmd);
        static void clean();
    };

    class LerApp
    {
    public:

        LerApp();
        ~LerApp();
        void run();
        [[nodiscard]] LerDevicePtr getDevice() const { return m_engine; }
        [[nodiscard]] const RenderPass& getRenderPass() const { return m_renderPass; }
        void show(const std::function<void()>& delegate);

        // Non-copyable and non-movable
        LerApp(const LerApp&) = delete;
        LerApp(const LerApp&&) = delete;
        LerApp& operator=(const LerApp&) = delete;
        LerApp& operator=(const LerApp&&) = delete;

    private:

        static vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes, bool vSync);
        static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
        static vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);
        SwapChain createSwapChain(vk::SurfaceKHR surface, uint32_t width, uint32_t height, bool vSync = true);

        GLFWwindow* m_window = nullptr;
        vk::UniqueInstance m_instance;
        uint32_t m_graphicsQueueFamily = UINT32_MAX;
        uint32_t m_transferQueueFamily = UINT32_MAX;
        vk::UniqueDevice m_device;
        vk::UniqueSurfaceKHR m_surface;
        vk::PhysicalDevice m_physicalDevice;
        vk::UniquePipelineCache m_pipelineCache;
        LerDevicePtr m_engine;
        SwapChain m_swapChain;
        RenderPass m_renderPass;
        vk::UniqueDescriptorPool m_imguiPool;

        std::list<std::function<void()>> m_printer;
    };
}

#endif //LER_H
