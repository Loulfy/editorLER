//
// Created by loulfy on 27/02/2023.
//

#include "ler.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace ler
{
    static const std::array<vk::Format,5> c_depthFormats =
    {
        vk::Format::eD32SfloatS8Uint,
        vk::Format::eD32Sfloat,
        vk::Format::eD24UnormS8Uint,
        vk::Format::eD16UnormS8Uint,
        vk::Format::eD16Unorm
    };

    LerDevice::LerDevice(const VulkanContext& context) : m_context(context)
    {
        // Create Command Pool
        auto poolUsage = vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient;
        m_commandPool = m_context.device.createCommandPoolUnique({ poolUsage, context.graphicsQueueFamily });
        m_queue = m_context.device.getQueue(context.graphicsQueueFamily, 0);
    }

    LerDevice::~LerDevice()
    {
        vmaDestroyAllocator(m_context.allocator);
    }

    BufferPtr LerDevice::createBuffer(uint32_t byteSize, vk::BufferUsageFlags usages, bool staging)
    {
        auto buffer = std::make_shared<Buffer>(m_context);
        vk::BufferUsageFlags usageFlags = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst;
        usageFlags |= usages;
        usageFlags |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
        usageFlags |= vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
        buffer->info = vk::BufferCreateInfo();
        buffer->info.setSize(byteSize);
        buffer->info.setUsage(usageFlags);
        buffer->info.setSharingMode(vk::SharingMode::eExclusive);

        buffer->allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        buffer->allocInfo.flags = staging ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        vmaCreateBuffer(m_context.allocator, reinterpret_cast<VkBufferCreateInfo*>(&buffer->info), &buffer->allocInfo, reinterpret_cast<VkBuffer*>(&buffer->handle), &buffer->allocation, nullptr);

        return buffer;
    }

    void LerDevice::uploadBuffer(BufferPtr& buffer, const void* src, uint32_t byteSize)
    {
        assert(src);
        if(buffer->isStaging() && buffer->length() >= byteSize)
        {
            void* dst = nullptr;
            vmaMapMemory(m_context.allocator, static_cast<VmaAllocation>(buffer->allocation), &dst);
            std::memcpy(dst, src, byteSize);
            vmaUnmapMemory(m_context.allocator, static_cast<VmaAllocation>(buffer->allocation));
        }
    }

    void LerDevice::copyBuffer(BufferPtr& src, BufferPtr& dst, uint64_t byteSize)
    {

    }

    static vk::ImageUsageFlags pickImageUsage(vk::Format format, bool isRenderTarget)
    {
        vk::ImageUsageFlags ret = vk::ImageUsageFlagBits::eTransferSrc |
                                  vk::ImageUsageFlagBits::eTransferDst |
                                  vk::ImageUsageFlagBits::eSampled;

        if (isRenderTarget)
        {
            ret |= vk::ImageUsageFlagBits::eInputAttachment;
            switch(format)
            {
                case vk::Format::eS8Uint:
                case vk::Format::eD16Unorm:
                case vk::Format::eD32Sfloat:
                case vk::Format::eD16UnormS8Uint:
                case vk::Format::eD24UnormS8Uint:
                case vk::Format::eD32SfloatS8Uint:
                case vk::Format::eX8D24UnormPack32:
                    ret |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
                    break;

                default:
                    ret |= vk::ImageUsageFlagBits::eColorAttachment;
                    ret |= vk::ImageUsageFlagBits::eStorage;
                    break;
            }
        }
        return ret;
    }

    vk::ImageAspectFlags LerDevice::guessImageAspectFlags(vk::Format format)
    {
        switch(format)
        {
            case vk::Format::eD16Unorm:
            case vk::Format::eX8D24UnormPack32:
            case vk::Format::eD32Sfloat:
                return vk::ImageAspectFlagBits::eDepth;

            case vk::Format::eS8Uint:
                return vk::ImageAspectFlagBits::eStencil;

            case vk::Format::eD16UnormS8Uint:
            case vk::Format::eD24UnormS8Uint:
            case vk::Format::eD32SfloatS8Uint:
                return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;

            default:
                return vk::ImageAspectFlagBits::eColor;
        }
    }

    vk::Format LerDevice::chooseDepthFormat()
    {
        for (const vk::Format& format : c_depthFormats)
        {
            vk::FormatProperties depthFormatProperties = m_context.physicalDevice.getFormatProperties(format);
            // Format must support depth stencil attachment for optimal tiling
            if (depthFormatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
                return format;
        }
        return vk::Format::eD32Sfloat;
    }

    void LerDevice::populateTexture(const TexturePtr& texture, vk::Format format, const vk::Extent2D& extent, vk::SampleCountFlagBits sampleCount, bool isRenderTarget)
    {
        texture->allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        texture->allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        texture->info = vk::ImageCreateInfo();
        texture->info.setImageType(vk::ImageType::e2D);
        texture->info.setExtent(vk::Extent3D(extent.width, extent.height, 1));
        texture->info.setMipLevels(1);
        texture->info.setArrayLayers(1);
        texture->info.setFormat(format);
        texture->info.setInitialLayout(vk::ImageLayout::eUndefined);
        texture->info.setUsage(pickImageUsage(format, isRenderTarget));
        texture->info.setSharingMode(vk::SharingMode::eExclusive);
        texture->info.setSamples(sampleCount);
        texture->info.setFlags({});
        texture->info.setTiling(vk::ImageTiling::eOptimal);

        vmaCreateImage(m_context.allocator, reinterpret_cast<VkImageCreateInfo*>(&texture->info), &texture->allocInfo, reinterpret_cast<VkImage*>(&texture->handle), &texture->allocation, nullptr);

        vk::ImageViewCreateInfo createInfo;
        createInfo.setImage(texture->handle);
        createInfo.setViewType(vk::ImageViewType::e2D);
        createInfo.setFormat(format);
        createInfo.setSubresourceRange(vk::ImageSubresourceRange(guessImageAspectFlags(format), 0, 1, 0, 1));
        texture->view = m_context.device.createImageViewUnique(createInfo);
    }

    TexturePtr LerDevice::createTexture(vk::Format format, const vk::Extent2D& extent, vk::SampleCountFlagBits sampleCount, bool isRenderTarget)
    {
        auto texture = std::make_shared<Texture>(m_context);
        populateTexture(texture, format, extent, sampleCount, isRenderTarget);
        return texture;
    }

    TexturePtr LerDevice::createTextureFromNative(vk::Image image, vk::Format format, const vk::Extent2D& extent)
    {
        auto texture = std::make_shared<Texture>(m_context);

        texture->info = vk::ImageCreateInfo();
        texture->info.setImageType(vk::ImageType::e2D);
        texture->info.setExtent(vk::Extent3D(extent.width, extent.height, 1));
        texture->info.setSamples(vk::SampleCountFlagBits::e1);
        texture->info.setFormat(format);

        texture->handle = image;
        texture->allocation = nullptr;

        vk::ImageViewCreateInfo createInfo;
        createInfo.setImage(image);
        createInfo.setViewType(vk::ImageViewType::e2D);
        createInfo.setFormat(format);
        createInfo.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
        texture->view = m_context.device.createImageViewUnique(createInfo);

        return texture;
    }

    RenderPass LerDevice::createDefaultRenderPass(vk::Format surfaceFormat)
    {
        RenderPass renderPass;
        renderPass.attachments.resize(6);
        std::array<vk::SubpassDescription2, 3> subPass;
        std::vector<vk::AttachmentReference2> colorAttachmentRef1(3);
        std::vector<vk::AttachmentReference2> colorAttachmentRef2(1);
        std::vector<vk::AttachmentReference2> colorInputRef(3);
        std::array<vk::AttachmentReference2,1> resolveAttachmentRef;
        vk::AttachmentReference2 depthAttachmentRef;

        auto properties = m_context.physicalDevice.getProperties();
        vk::SampleCountFlagBits sampleCount;
        vk::SampleCountFlags samples = std::min(properties.limits.framebufferColorSampleCounts, properties.limits.framebufferDepthSampleCounts);
        if(samples & vk::SampleCountFlagBits::e1) sampleCount = vk::SampleCountFlagBits::e1;
        if(samples & vk::SampleCountFlagBits::e8) sampleCount = vk::SampleCountFlagBits::e8;

        // Position & Normal
        for(uint32_t i = 0; i < 2; i++)
        {
            renderPass.attachments[i] = vk::AttachmentDescription2()
                .setFormat(vk::Format::eR16G16B16A16Sfloat)
                .setSamples(sampleCount)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

            colorAttachmentRef1[i] = vk::AttachmentReference2()
                .setAttachment(i)
                .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

            colorInputRef[i] = vk::AttachmentReference2()
                .setAttachment(i)
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        }

        // Albedo + Specular
        renderPass.attachments[2] = vk::AttachmentDescription2()
            .setFormat(vk::Format::eR8G8B8A8Unorm)
            .setSamples(sampleCount)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

        colorAttachmentRef1[2] = vk::AttachmentReference2()
            .setAttachment(2)
            .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        colorInputRef[2] = vk::AttachmentReference2()
            .setAttachment(2)
            .setAspectMask(vk::ImageAspectFlagBits::eColor)
            .setLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        // Depth + Stencil
        vk::ImageLayout depthLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        renderPass.attachments[3] = vk::AttachmentDescription2()
            .setFormat(chooseDepthFormat())
            .setSamples(sampleCount)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setStencilLoadOp(vk::AttachmentLoadOp::eClear)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(depthLayout);

        depthAttachmentRef = vk::AttachmentReference2()
            .setAttachment(3)
            .setLayout(depthLayout);

        // Result Color Image (DIRECT)
        renderPass.attachments[4] = vk::AttachmentDescription2()
            .setFormat(vk::Format::eB8G8R8A8Unorm)
            .setSamples(sampleCount)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

        colorAttachmentRef2[0] = vk::AttachmentReference2()
            .setAttachment(4)
            .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        // Resolve Present Image
        renderPass.attachments[5] = vk::AttachmentDescription2()
            .setFormat(surfaceFormat)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

        resolveAttachmentRef[0] = vk::AttachmentReference2()
            .setAttachment(5)
            .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        // FIRST PASS
        subPass[0] = vk::SubpassDescription2()
            .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
            .setColorAttachments(colorAttachmentRef1)
            .setPDepthStencilAttachment(&depthAttachmentRef);

        for(auto& output : colorAttachmentRef1)
            renderPass.subPass[0].insert(output.attachment);

        // SECOND PASS
        subPass[1] = vk::SubpassDescription2()
            .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
            .setColorAttachments(colorAttachmentRef2)
            //.setResolveAttachments(resolveAttachmentRef)
            .setPDepthStencilAttachment(&depthAttachmentRef)
            .setInputAttachments(colorInputRef);

        for(auto& output : colorAttachmentRef2)
            renderPass.subPass[1].insert(output.attachment);

        // THIRD PASS
        subPass[2] = vk::SubpassDescription2()
            .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
            .setColorAttachments(colorAttachmentRef2)
            .setResolveAttachments(resolveAttachmentRef)
            .setPDepthStencilAttachment(&depthAttachmentRef);

        for(auto& output : colorAttachmentRef2)
            renderPass.subPass[2].insert(output.attachment);

        // DEPENDENCIES
        std::array<vk::SubpassDependency2, 2> dependencies;
        dependencies[0] = vk::SubpassDependency2()
            .setSrcSubpass(0)
            .setDstSubpass(1)
            .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
            .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
            .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
            .setDstAccessMask(vk::AccessFlagBits::eShaderRead);

        dependencies[1] = vk::SubpassDependency2()
            .setSrcSubpass(1)
            .setDstSubpass(2)
            .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
            .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
            .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
            .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

        vk::RenderPassCreateInfo2 renderPassInfo;
        renderPassInfo.setAttachments(renderPass.attachments);
        renderPassInfo.setDependencies(dependencies);
        renderPassInfo.setSubpasses(subPass);

        renderPass.handle = m_context.device.createRenderPass2Unique(renderPassInfo);
        return renderPass;
    }

    std::vector<FrameBuffer> LerDevice::createFrameBuffers(const RenderPass& renderPass, const SwapChain& swapChain)
    {
        std::vector<FrameBuffer> frameBuffers;
        std::vector<vk::ImageView> attachmentViews;
        attachmentViews.reserve(renderPass.attachments.size());
        auto swapChainImages = m_context.device.getSwapchainImagesKHR(swapChain.handle.get());
        for(auto& image : swapChainImages)
        {
            auto& frameBuffer = frameBuffers.emplace_back();
            attachmentViews.clear();
            for(size_t i = 0; i < renderPass.attachments.size() - 1; ++i)
            {
                auto texture = createTexture(renderPass.attachments[i].format, swapChain.extent, renderPass.attachments[i].samples, true);
                attachmentViews.emplace_back(texture->view.get());
                frameBuffer.images.push_back(texture);
            }
            auto frame = createTextureFromNative(image, swapChain.format, swapChain.extent);
            attachmentViews.emplace_back(frame->view.get());
            frameBuffer.images.push_back(frame);

            vk::FramebufferCreateInfo framebufferInfo;
            framebufferInfo.setRenderPass(renderPass.handle.get());
            framebufferInfo.setAttachments(attachmentViews);
            framebufferInfo.setWidth(swapChain.extent.width);
            framebufferInfo.setHeight(swapChain.extent.height);
            framebufferInfo.setLayers(1);

            frameBuffer.handle = m_context.device.createFramebufferUnique(framebufferInfo);
        }

        return frameBuffers;
    }

    vk::CommandBuffer LerDevice::getCommandBuffer()
    {
        vk::CommandBuffer cmd;
        if (m_commandBuffersPool.empty())
        {
            // Allocate command buffer
            auto allocInfo = vk::CommandBufferAllocateInfo();
            allocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
            allocInfo.setCommandPool(m_commandPool.get());
            allocInfo.setCommandBufferCount(1);

            vk::Result res;
            res = m_context.device.allocateCommandBuffers(&allocInfo, &cmd);
            assert(res == vk::Result::eSuccess);
        }
        else
        {
            cmd = m_commandBuffersPool.front();
            m_commandBuffersPool.pop_front();
        }

        cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        return cmd;
    }

    void LerDevice::submitAndWait(vk::CommandBuffer& cmd)
    {
        cmd.end();
        vk::UniqueFence fence = m_context.device.createFenceUnique({});

        vk::SubmitInfo submitInfo;
        submitInfo.setCommandBuffers(cmd);
        m_mutexQueue.lock();
        m_queue.submit(submitInfo, fence.get());
        m_mutexQueue.unlock();

        auto res = m_context.device.waitForFences(fence.get(), true, std::numeric_limits<uint64_t>::max());
        assert(res == vk::Result::eSuccess);

        m_commandBuffersPool.push_back(cmd);
    }
}