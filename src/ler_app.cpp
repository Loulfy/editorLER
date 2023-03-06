//
// Created by loulfy on 22/02/2023.
//

#define VMA_IMPLEMENTATION
#include "ler.hpp"

static const uint32_t WIDTH = 1280;
static const uint32_t HEIGHT = 720;

namespace ler
{
    LerApp::LerApp()
    {
        if (!glfwInit())
            throw std::runtime_error("failed to init glfw");

        // Create window
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_window = glfwCreateWindow(WIDTH, HEIGHT, "LER", nullptr, nullptr);
        //glfwSetKeyCallback(window, glfw_key_callback);
        //glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        uint32_t count;
        const char** extensions = glfwGetRequiredInstanceExtensions(&count);
        std::vector<const char*> instanceExtensions(extensions, extensions + count);

        static const vk::DynamicLoader dl;
        const auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        instanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        instanceExtensions.emplace_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        instanceExtensions.emplace_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
        instanceExtensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

        std::initializer_list<const char*> layers = {
            "VK_LAYER_KHRONOS_validation"
        };

        std::vector<const char*> devices = {
            VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
            VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
            // VULKAN MEMORY ALLOCATOR
            VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
            VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
            // SWAP CHAIN
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            // RAY TRACING
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        };

        // Create instance
        vk::ApplicationInfo appInfo;
        appInfo.setApiVersion(VK_API_VERSION_1_3);
        appInfo.setPEngineName("LER");

        vk::InstanceCreateInfo instInfo;
        instInfo.setPApplicationInfo(&appInfo);
        instInfo.setPEnabledLayerNames(layers);
        instInfo.setPEnabledExtensionNames(instanceExtensions);
        m_instance = vk::createInstanceUnique(instInfo);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance.get());

        // Pick First GPU
        m_physicalDevice = m_instance->enumeratePhysicalDevices().front();
        std::cout << "GPU: " << m_physicalDevice.getProperties().deviceName << std::endl;

        std::set<std::string> supportedExtensionSet;
        for(auto& extProp : m_physicalDevice.enumerateDeviceExtensionProperties())
            supportedExtensionSet.insert(extProp.extensionName);

        bool supportRayTracing = supportedExtensionSet.contains(VK_KHR_RAY_QUERY_EXTENSION_NAME);
        std::cout << "Support Ray Tracing: " << std::boolalpha << supportRayTracing << std::endl;

        if(supportRayTracing)
        {
            devices.emplace_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
            devices.emplace_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
        }

        // Device Features
        auto features = m_physicalDevice.getFeatures();

        // Find Graphics Queue
        const auto queueFamilies = m_physicalDevice.getQueueFamilyProperties();
        const auto family = std::find_if(queueFamilies.begin(), queueFamilies.end(), [](const vk::QueueFamilyProperties& queueFamily) {
             return queueFamily.queueCount > 0 && queueFamily.queueFlags & vk::QueueFlagBits::eGraphics;
        });

        m_graphicsQueueFamily = std::distance(queueFamilies.begin(), family);
        m_transferQueueFamily = UINT32_MAX;

        // Find Transfer Queue (for parallel command)
        for(size_t i = 0; i < queueFamilies.size(); ++i)
        {
            if(queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & vk::QueueFlagBits::eTransfer && m_graphicsQueueFamily != i)
            {
                m_transferQueueFamily = i;
                break;
            }
        }

        if(m_transferQueueFamily == UINT32_MAX)
            throw std::runtime_error("No transfer queue available");

        // Create queues
        float queuePriority = 1.0f;
        std::initializer_list<vk::DeviceQueueCreateInfo> queueCreateInfos = {
            { {}, m_graphicsQueueFamily, 1, &queuePriority },
            { {}, m_transferQueueFamily, 1, &queuePriority },
        };

        for(auto& q : queueCreateInfos)
            std::cout << "Queue Family " << q.queueFamilyIndex << ": " << vk::to_string(queueFamilies[q.queueFamilyIndex].queueFlags) << std::endl;

        // Create Device
        vk::DeviceCreateInfo deviceInfo;
        deviceInfo.setQueueCreateInfos(queueCreateInfos);
        deviceInfo.setPEnabledExtensionNames(devices);
        deviceInfo.setPEnabledLayerNames(layers);
        deviceInfo.setPEnabledFeatures(&features);

        vk::PhysicalDeviceVulkan11Features vulkan11Features;
        vulkan11Features.setShaderDrawParameters(true);
        vk::PhysicalDeviceVulkan12Features vulkan12Features;

        vulkan12Features.setDescriptorIndexing(true);
        vulkan12Features.setRuntimeDescriptorArray(true);
        vulkan12Features.setDescriptorBindingPartiallyBound(true);
        vulkan12Features.setDescriptorBindingVariableDescriptorCount(true);
        vulkan12Features.setTimelineSemaphore(true);
        vulkan12Features.setBufferDeviceAddress(true);
        vulkan12Features.setShaderSampledImageArrayNonUniformIndexing(true);

        vulkan12Features.setBufferDeviceAddress(true);
        vulkan12Features.setRuntimeDescriptorArray(true);
        vulkan12Features.setDescriptorBindingVariableDescriptorCount(true);
        vulkan12Features.setShaderSampledImageArrayNonUniformIndexing(true);
        vk::StructureChain<vk::DeviceCreateInfo,
        vk::PhysicalDeviceRayQueryFeaturesKHR,
        /*vk::PhysicalDeviceRayTracingPipelineFeaturesKHR,*/
        vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
        vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan12Features> createInfoChain(deviceInfo, {supportRayTracing}, {supportRayTracing}, /*{false},*/ vulkan11Features, vulkan12Features);
        m_device = m_physicalDevice.createDeviceUnique(createInfoChain.get<vk::DeviceCreateInfo>());
        VULKAN_HPP_DEFAULT_DISPATCHER.init(m_device.get());

        // Create surface
        VkSurfaceKHR glfwSurface;
        auto res = glfwCreateWindowSurface(m_instance.get(), m_window, nullptr, &glfwSurface);
        if (res != VK_SUCCESS)
            throw std::runtime_error("failed to create window surface!");

        m_surface = vk::UniqueSurfaceKHR(vk::SurfaceKHR(glfwSurface), { m_instance.get() });
        m_pipelineCache = m_device->createPipelineCacheUnique({});

        // Create Memory Allocator
        VmaAllocatorCreateInfo allocatorCreateInfo = {};
        allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        allocatorCreateInfo.device = m_device.get();
        allocatorCreateInfo.instance = m_instance.get();
        allocatorCreateInfo.physicalDevice = m_physicalDevice;
        allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
        allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

        VmaAllocator allocator;
        vmaCreateAllocator(&allocatorCreateInfo, &allocator);

        VulkanContext context;
        context.device = m_device.get();
        context.instance = m_instance.get();
        context.physicalDevice = m_physicalDevice;
        context.graphicsQueueFamily = m_graphicsQueueFamily;
        context.transferQueueFamily = m_transferQueueFamily;
        context.pipelineCache = m_pipelineCache.get();
        context.allocator = allocator;

        // Create Engine Instance
        m_engine = std::make_shared<LerDevice>(context);
    }

    LerApp::~LerApp()
    {
        m_device->waitIdle();
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    vk::PresentModeKHR LerApp::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes, bool vSync)
    {
        for (const auto& availablePresentMode : availablePresentModes)
        {
            if(availablePresentMode == vk::PresentModeKHR::eFifo && vSync)
                return availablePresentMode;
            if (availablePresentMode == vk::PresentModeKHR::eMailbox && !vSync)
                return availablePresentMode;
        }
        return vk::PresentModeKHR::eImmediate;
    }

    vk::SurfaceFormatKHR LerApp::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
    {
        if (availableFormats.size() == 1 && availableFormats[0].format == vk::Format::eUndefined)
        {
            return { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
        }

        for (const auto& format : availableFormats)
        {
            if (format.format == vk::Format::eB8G8R8A8Unorm && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                return format;
        }

        throw std::runtime_error("found no suitable surface format");
        return {};
    }

    vk::Extent2D LerApp::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height)
    {
        if (capabilities.currentExtent.width == UINT32_MAX)
        {
            vk::Extent2D extent(width, height);
            vk::Extent2D minExtent = capabilities.minImageExtent;
            vk::Extent2D maxExtent = capabilities.maxImageExtent;
            extent.width = std::clamp(extent.width, minExtent.width, maxExtent.width);
            extent.height = std::clamp(extent.height, minExtent.height, maxExtent.height);
            return extent;
        }
        else
        {
            return capabilities.currentExtent;
        }
    }

    SwapChain LerApp::createSwapChain(vk::SurfaceKHR surface, uint32_t width, uint32_t height, bool vSync)
    {
        // Setup viewports, vSync
        std::vector<vk::SurfaceFormatKHR> surfaceFormats = m_physicalDevice.getSurfaceFormatsKHR(surface);
        vk::SurfaceCapabilitiesKHR surfaceCapabilities = m_physicalDevice.getSurfaceCapabilitiesKHR(surface);
        std::vector<vk::PresentModeKHR> surfacePresentModes = m_physicalDevice.getSurfacePresentModesKHR(surface);

        vk::Extent2D extent = chooseSwapExtent(surfaceCapabilities, width, height);
        vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(surfaceFormats);
        vk::PresentModeKHR presentMode = chooseSwapPresentMode(surfacePresentModes, vSync);

        uint32_t backBufferCount = std::clamp(surfaceCapabilities.maxImageCount, 1U, 2U);

        // Create swapChain
        using vkIU = vk::ImageUsageFlagBits;
        vk::SwapchainCreateInfoKHR createInfo;
        createInfo.setSurface(surface);
        createInfo.setMinImageCount(backBufferCount);
        createInfo.setImageFormat(surfaceFormat.format);
        createInfo.setImageColorSpace(surfaceFormat.colorSpace);
        createInfo.setImageExtent(extent);
        createInfo.setImageArrayLayers(1);
        createInfo.setImageUsage(vkIU::eColorAttachment | vkIU::eTransferDst | vkIU::eSampled);
        createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
        createInfo.setPreTransform(surfaceCapabilities.currentTransform);
        createInfo.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
        createInfo.setPresentMode(presentMode);
        createInfo.setClipped(true);

        std::cout << "SwapChain: Images(" << backBufferCount
        << "), Extent(" << extent.width << "x" << extent.height
        << "), Format(" << vk::to_string(surfaceFormat.format)
        << "), Present(" << vk::to_string(presentMode) << ")"
        << std::endl;

        SwapChain swapChain;
        swapChain.format = surfaceFormat.format;
        swapChain.extent = extent;
        swapChain.handle = m_device->createSwapchainKHRUnique(createInfo);
        return swapChain;
    }

    void LerApp::run()
    {
        vk::Result result;
        vk::CommandBuffer cmd;
        uint32_t swapChainIndex = 0;
        auto queue = m_device->getQueue(m_graphicsQueueFamily, 0);
        auto presentSemaphore = m_device->createSemaphoreUnique({});
        auto swapChain = createSwapChain(m_surface.get(), WIDTH, HEIGHT);
        auto renderPass = m_engine->createDefaultRenderPass(swapChain.format);
        auto frameBuffers = m_engine->createFrameBuffers(renderPass, swapChain);

        // PREPARE RenderPass
        std::array<float, 4> color = {1.f, 1.f, 1.f, 1.f};
        std::vector<vk::ClearValue> clearValues;
        for(const auto& attachment : renderPass.attachments)
        {
            auto aspect = LerDevice::guessImageAspectFlags(attachment.format);
            if(aspect == vk::ImageAspectFlagBits::eColor)
                clearValues.emplace_back(vk::ClearColorValue(color));
            else
                clearValues.emplace_back(vk::ClearDepthStencilValue(1.0f, 0));
        }

        vk::Viewport viewport(0, 0, static_cast<float>(swapChain.extent.width), static_cast<float>(swapChain.extent.height), 0, 1.0f);
        vk::Rect2D renderArea(vk::Offset2D(), swapChain.extent);

        // PREPARE ImGui
        auto imguiPool = ImguiImpl::createPool(m_engine);
        ImguiImpl::init(m_engine, imguiPool.get(), renderPass, m_window);

        while(!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();

            // Acquire next frame
            result = m_device->acquireNextImageKHR(swapChain.handle.get(), std::numeric_limits<uint64_t>::max(), presentSemaphore.get(), vk::Fence(), &swapChainIndex);
            assert(result == vk::Result::eSuccess);

            // Render
            cmd = m_engine->getCommandBuffer();

            // Begin RenderPass
            vk::RenderPassBeginInfo beginInfo;
            beginInfo.setRenderPass(renderPass.handle.get());
            beginInfo.setFramebuffer(frameBuffers[swapChainIndex].handle.get());
            beginInfo.setRenderArea(renderArea);
            beginInfo.setClearValues(clearValues);
            cmd.beginRenderPass(beginInfo, vk::SubpassContents::eInline);
            cmd.nextSubpass(vk::SubpassContents::eInline);
            cmd.nextSubpass(vk::SubpassContents::eInline);

            ImguiImpl::begin();
            ImGui::ShowDemoWindow();
            for(auto& print : m_printer)
                print();
            ImguiImpl::end(cmd);

            cmd.endRenderPass();
            m_engine->submitAndWait(cmd);

            // Present
            vk::PresentInfoKHR presentInfo;
            presentInfo.setWaitSemaphoreCount(1);
            presentInfo.setPWaitSemaphores(&presentSemaphore.get());
            presentInfo.setSwapchainCount(1);
            presentInfo.setPSwapchains(&swapChain.handle.get());
            presentInfo.setPImageIndices(&swapChainIndex);

            result = queue.presentKHR(&presentInfo);
            assert(result == vk::Result::eSuccess);
        }

        ImguiImpl::clean();
    }

    void LerApp::show(const std::function<void()>& delegate)
    {
        m_printer.push_back(delegate);
    }
}