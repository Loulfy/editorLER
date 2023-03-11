//
// Created by loulfy on 22/02/2023.
//

#ifndef LER_H
#define LER_H

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

#define GLFW_INCLUDE_NONE // Do not include any OpenGL/Vulkan headers
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>

#include "ler_sys.hpp"
#include "ler_spv.hpp"

namespace ler
{
    struct VulkanContext
    {
        vk::Instance instance;
        vk::PhysicalDevice physicalDevice;
        vk::Device device;
        uint32_t graphicsQueueFamily = UINT32_MAX;
        uint32_t transferQueueFamily = UINT32_MAX;
        VmaAllocator allocator = nullptr;
        vk::PipelineCache pipelineCache;
    };

    struct Buffer
    {
        vk::Buffer handle;
        vk::BufferCreateInfo info;
        VmaAllocation allocation = nullptr;
        VmaAllocationCreateInfo allocInfo = {};

        ~Buffer() { vmaDestroyBuffer(m_context.allocator, static_cast<VkBuffer>(handle), allocation); }
        explicit Buffer(const VulkanContext& context) : m_context(context) { }
        [[nodiscard]] uint32_t length() const { return info.size; }
        [[nodiscard]] bool isStaging() const { return allocInfo.flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT; }

    private:

        const VulkanContext& m_context;
    };

    using BufferPtr = std::shared_ptr<Buffer>;

    struct Texture
    {
        vk::Image handle;
        vk::ImageCreateInfo info;
        vk::UniqueImageView view;
        VmaAllocation allocation = nullptr;
        VmaAllocationCreateInfo allocInfo = {};

        ~Texture() { if(allocation) vmaDestroyImage(m_context.allocator, static_cast<VkImage>(handle), allocation); }
        explicit Texture(const VulkanContext& context) : m_context(context) { }

    private:

        const VulkanContext& m_context;
    };

    using TexturePtr = std::shared_ptr<Texture>;

    struct SwapChain
    {
        vk::UniqueSwapchainKHR handle;
        vk::Format format = vk::Format::eB8G8R8A8Unorm;
        vk::Extent2D extent;
    };

    struct RenderPass
    {
        vk::UniqueRenderPass handle;
        std::vector<vk::AttachmentDescription2> attachments;
        std::array<std::set<uint32_t>, 4> subPass;
    };

    struct FrameBuffer
    {
        vk::UniqueFramebuffer handle;
        std::vector<TexturePtr> images;
    };

    struct DescriptorSetLayoutData
    {
        uint32_t set_number = 0;
        VkDescriptorSetLayoutCreateInfo create_info;
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
    };

    struct Shader
    {
        vk::UniqueShaderModule shaderModule;
        vk::ShaderStageFlagBits stageFlagBits = {};
        vk::PipelineVertexInputStateCreateInfo pvi;
        std::vector<vk::PushConstantRange> pushConstants;
        std::map<uint32_t, DescriptorSetLayoutData> descriptorMap;
        std::vector<vk::VertexInputBindingDescription> bindingDesc;
        std::vector<vk::VertexInputAttributeDescription> attributeDesc;
    };

    using ShaderPtr = std::shared_ptr<Shader>;

    struct DescriptorAllocator
    {
        std::vector<vk::DescriptorSetLayoutBinding> layoutBinding;
        vk::UniqueDescriptorSetLayout layout;
        vk::UniqueDescriptorPool pool;
    };

    struct PipelineInfo
    {
        vk::Extent2D extent;
        vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
        vk::PolygonMode polygonMode = vk::PolygonMode::eFill;
        vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;
        uint32_t textureCount = 1;
        bool writeDepth = true;
        uint32_t subPass = 0;
    };

    class BasePipeline
    {
    public:

        void reflectPipelineLayout(vk::Device device, const std::vector<ShaderPtr>& shaders);
        vk::DescriptorSet createDescriptorSet(vk::Device& device, uint32_t set);

        vk::UniquePipeline handle;
        vk::UniquePipelineLayout pipelineLayout;
        vk::PipelineBindPoint bindPoint = vk::PipelineBindPoint::eGraphics;
        std::unordered_map<uint32_t,DescriptorAllocator> descriptorAllocMap;
    };

    using PipelinePtr = std::shared_ptr<BasePipeline>;

    class GraphicsPipeline : public BasePipeline
    {

    };

    class ComputePipeline : public BasePipeline
    {

    };

    class LerDevice
    {
    public:

        virtual ~LerDevice();
        explicit LerDevice(const VulkanContext& context);

        // Buffer
        BufferPtr createBuffer(uint32_t byteSize, vk::BufferUsageFlags usages = vk::BufferUsageFlagBits(), bool staging = false);
        void uploadBuffer(BufferPtr& staging, const void* src, uint32_t byteSize);
        void copyBuffer(BufferPtr& src, BufferPtr& dst, uint64_t byteSize = VK_WHOLE_SIZE);
        static void copyBufferToTexture(vk::CommandBuffer& cmd, const BufferPtr& buffer, const TexturePtr& texture);

        // Texture
        TexturePtr createTexture(vk::Format format, const vk::Extent2D& extent, vk::SampleCountFlagBits sampleCount, bool isRenderTarget = false);
        TexturePtr createTextureFromNative(vk::Image image, vk::Format format, const vk::Extent2D& extent);
        vk::UniqueSampler createSampler(const vk::SamplerAddressMode& addressMode, bool filter);
        TexturePtr loadTextureFromFile(const fs::path& path);
        static vk::ImageAspectFlags guessImageAspectFlags(vk::Format format);

        // RenderPass
        RenderPass createDefaultRenderPass(vk::Format surfaceFormat);
        std::vector<FrameBuffer> createFrameBuffers(const RenderPass& renderPass, const SwapChain& swapChain);
        RenderPass createSimpleRenderPass(vk::Format surfaceFormat);
        FrameBuffer createFrameBuffer(const RenderPass& renderPass, const vk::Extent2D& extent);
        static std::vector<vk::ClearValue> clearRenderPass(const RenderPass& renderPass);

        // Pipeline
        ShaderPtr createShader(const fs::path& path);
        PipelinePtr createGraphicsPipeline(const RenderPass& renderPass, const std::vector<ShaderPtr>& shaders, const PipelineInfo& info);
        PipelinePtr createComputePipeline(const ShaderPtr& shader);

        // Execution
        vk::CommandBuffer getCommandBuffer();
        void submitAndWait(vk::CommandBuffer& cmd);

        [[nodiscard]] const VulkanContext& getVulkanContext() const { return m_context; }

    private:

        void populateTexture(const TexturePtr& texture, vk::Format format, const vk::Extent2D& extent, vk::SampleCountFlagBits sampleCount, bool isRenderTarget = false);
        static uint32_t formatSize(VkFormat format);
        vk::Format chooseDepthFormat();
        static std::vector<char> loadBinaryFromFile(const fs::path& path);

        VulkanContext m_context;
        std::mutex m_mutexQueue;
        vk::Queue m_queue;
        vk::UniqueCommandPool m_commandPool;
        std::list<vk::CommandBuffer> m_commandBuffersPool;
    };

    using LerDevicePtr = std::shared_ptr<LerDevice>;

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
