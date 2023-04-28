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
#include "ler_cam.hpp"
#include "ler_arc.hpp"
#include "ler_rdr.hpp"
#include "ler_res.hpp"

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
        void bindController(std::shared_ptr<Controller>& controller) { m_controller.push_back(controller); }
        void lockCursor() { glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); }
        [[nodiscard]] bool isCursorLock() const { return glfwGetInputMode(m_window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED; }

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

        static void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void glfw_mouse_callback(GLFWwindow* window, int button, int action, int mods);
        static void glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

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

        std::vector<ControllerPtr> m_controller;
        std::list<std::function<void()>> m_printer;
    };

    class MeshViewer
    {
    public:

        void init(LerDevicePtr& device);
        void display(LerDevicePtr& device, BatchedMesh& batch);
        void switchMesh(const BatchedMesh& batch, int id);

    private:

        RenderTargetPtr m_renderTarget;
        MeshRenderer m_meshRenderer;
        BoxRenderer m_boxRenderer;
        vk::UniqueSampler m_sampler;
        SceneConstant m_constant;
        VkDescriptorSet m_ds;
        ArcCamera m_camera;
        int m_id = 0;
        int m_own = 0;

        static int counter;
    };
}

#endif //LER_H
