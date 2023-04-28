//
// Created by loulfy on 27/02/2023.
//

#include "ler.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#define SPIRV_REFLECT_HAS_VULKAN_H
#include <spirv_reflect.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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

    static const std::array<std::set<std::string>, 5> c_VertexAttrMap =
    {{
             {"inPos"},
             {"inTex", "inUV"},
             {"inNormal"},
             {"inTangent"},
             {"inColor"}
     }};

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

    void LerDevice::copyBuffer(BufferPtr& src, BufferPtr& dst, uint64_t byteSize, uint64_t dstOffset)
    {
        vk::CommandBuffer cmd = getCommandBuffer();
        vk::BufferCopy copyRegion(0, dstOffset, byteSize);
        cmd.copyBuffer(src->handle, dst->handle, copyRegion);
        submitAndWait(cmd);
    }

    void LerDevice::copyBufferToTexture(vk::CommandBuffer& cmd, const BufferPtr& buffer, const TexturePtr& texture)
    {
        // prepare texture to transfer layout!
        std::vector<vk::ImageMemoryBarrier> imageBarriersStart;
        vk::PipelineStageFlags beforeStageFlags = vk::PipelineStageFlagBits::eTopOfPipe;
        vk::PipelineStageFlags afterStageFlags = vk::PipelineStageFlagBits::eTransfer;

        imageBarriersStart.emplace_back(
            vk::AccessFlags(),
            vk::AccessFlagBits::eTransferWrite,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            texture->handle,
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
        );

        cmd.pipelineBarrier(beforeStageFlags, afterStageFlags, vk::DependencyFlags(), {}, {}, imageBarriersStart);

        // Copy buffer to texture
        vk::BufferImageCopy copyRegion(0, 0, 0);
        copyRegion.imageExtent = texture->info.extent;
        copyRegion.imageSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
        cmd.copyBufferToImage(buffer->handle, texture->handle, vk::ImageLayout::eTransferDstOptimal, 1, &copyRegion);

        // prepare texture to color layout
        std::vector<vk::ImageMemoryBarrier> imageBarriersStop;
        beforeStageFlags = vk::PipelineStageFlagBits::eTransfer;
        afterStageFlags = vk::PipelineStageFlagBits::eAllCommands; //eFragmentShader
        imageBarriersStop.emplace_back(
            vk::AccessFlagBits::eTransferWrite,
            vk::AccessFlagBits::eShaderRead,
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            texture->handle,
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
        );

        cmd.pipelineBarrier(beforeStageFlags, afterStageFlags, vk::DependencyFlags(), {}, {}, imageBarriersStop);
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

    vk::UniqueSampler LerDevice::createSampler(const vk::SamplerAddressMode& addressMode, bool filter)
    {
        vk::SamplerCreateInfo samplerInfo;
        samplerInfo.setMagFilter(filter ? vk::Filter::eLinear : vk::Filter::eNearest);
        samplerInfo.setMinFilter(filter ? vk::Filter::eLinear : vk::Filter::eNearest);
        samplerInfo.setMipmapMode(filter ? vk::SamplerMipmapMode::eLinear : vk::SamplerMipmapMode::eNearest);
        samplerInfo.setAddressModeU(addressMode);
        samplerInfo.setAddressModeV(addressMode);
        samplerInfo.setAddressModeW(addressMode);
        samplerInfo.setMipLodBias(0.f);
        samplerInfo.setAnisotropyEnable(false);
        samplerInfo.setMaxAnisotropy(1.f);
        samplerInfo.setCompareEnable(false);
        samplerInfo.setCompareOp(vk::CompareOp::eLess);
        samplerInfo.setMinLod(0.f);
        samplerInfo.setMaxLod(std::numeric_limits<float>::max());
        samplerInfo.setBorderColor(vk::BorderColor::eFloatOpaqueBlack);

        return m_context.device.createSamplerUnique(samplerInfo, nullptr);
    }

    TexturePtr LerDevice::loadTextureFromFile(const fs::path& path)
    {
        int w, h, c;
        if(path.extension() == ".ktx" || path.extension() == ".dds")
            throw std::runtime_error("Can't load image with extension " + path.extension().string());

        auto blob = FileSystemService::Get().readFile(path);
        auto buff = reinterpret_cast<stbi_uc*>(blob.data());
        unsigned char* image = stbi_load_from_memory(buff, static_cast<int>(blob.size()), &w, &h, &c, STBI_rgb_alpha);
        //deprecated
        //unsigned char* image = stbi_load(path.string().c_str(), &w, &h, &c, STBI_rgb_alpha);
        size_t imageSize = w * h * 4;

        auto staging = createBuffer(imageSize, vk::BufferUsageFlags(), true);
        uploadBuffer(staging, image, imageSize);

        auto texture = createTexture(vk::Format::eR8G8B8A8Unorm, vk::Extent2D(w, h), vk::SampleCountFlagBits::e1);
        auto cmd = getCommandBuffer();
        copyBufferToTexture(cmd, staging, texture);
        submitAndWait(cmd);

        stbi_image_free(image);
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

    RenderPass LerDevice::createSimpleRenderPass(vk::Format surfaceFormat)
    {
        RenderPass renderPass;
        renderPass.attachments.resize(2);

        // Color Image
        renderPass.attachments[0] = vk::AttachmentDescription2()
            .setFormat(surfaceFormat)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        vk::AttachmentReference2 colorAttachmentRef = vk::AttachmentReference2()
            .setAttachment(0)
            .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

        // Depth + Stencil
        vk::ImageLayout depthLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        renderPass.attachments[1] = vk::AttachmentDescription2()
            .setFormat(chooseDepthFormat())
            .setSamples(vk::SampleCountFlagBits::e1)
            .setLoadOp(vk::AttachmentLoadOp::eClear)
            .setStoreOp(vk::AttachmentStoreOp::eStore)
            .setStencilLoadOp(vk::AttachmentLoadOp::eClear)
            .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
            .setInitialLayout(vk::ImageLayout::eUndefined)
            .setFinalLayout(depthLayout);

        vk::AttachmentReference2 depthAttachmentRef = vk::AttachmentReference2()
            .setAttachment(1)
            .setLayout(depthLayout);

        // One Pass
        vk::SubpassDescription2 subPass;
        subPass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
        subPass.setColorAttachments(colorAttachmentRef);
        subPass.setPDepthStencilAttachment(&depthAttachmentRef);

        renderPass.subPass[0].insert(colorAttachmentRef.attachment);

        // DEPENDENCIES
        std::array<vk::SubpassDependency2, 2> dependencies;
        dependencies[0] = vk::SubpassDependency2()
            .setSrcSubpass(VK_SUBPASS_EXTERNAL)
            .setDstSubpass(0)
            .setSrcStageMask(vk::PipelineStageFlagBits::eFragmentShader)
            .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
            .setSrcAccessMask(vk::AccessFlagBits::eShaderRead)
            .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion);

        dependencies[1] = vk::SubpassDependency2()
            .setSrcSubpass(0)
            .setDstSubpass(VK_SUBPASS_EXTERNAL)
            .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
            .setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader)
            .setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
            .setDstAccessMask(vk::AccessFlagBits::eShaderRead)
            .setDependencyFlags(vk::DependencyFlagBits::eByRegion);

        vk::RenderPassCreateInfo2 renderPassInfo;
        renderPassInfo.setAttachments(renderPass.attachments);
        renderPassInfo.setDependencies(dependencies);
        renderPassInfo.setSubpasses(subPass);

        renderPass.handle = m_context.device.createRenderPass2Unique(renderPassInfo);
        return renderPass;
    }

    FrameBuffer LerDevice::createFrameBuffer(const RenderPass& renderPass, const vk::Extent2D& extent)
    {
        FrameBuffer frameBuffer;
        std::vector<vk::ImageView> attachmentViews;
        attachmentViews.reserve(renderPass.attachments.size());
        for(const auto& attachment : renderPass.attachments)
        {
            auto texture = createTexture(attachment.format, extent, attachment.samples, true);
            attachmentViews.emplace_back(texture->view.get());
            frameBuffer.images.push_back(texture);
        }

        vk::FramebufferCreateInfo framebufferInfo;
        framebufferInfo.setRenderPass(renderPass.handle.get());
        framebufferInfo.setAttachments(attachmentViews);
        framebufferInfo.setWidth(extent.width);
        framebufferInfo.setHeight(extent.height);
        framebufferInfo.setLayers(1);

        frameBuffer.handle = m_context.device.createFramebufferUnique(framebufferInfo);

        return frameBuffer;
    }

    std::vector<vk::ClearValue> LerDevice::clearRenderPass(const RenderPass& renderPass, const std::array<float, 4>& color)
    {
        std::vector<vk::ClearValue> clearValues;
        for(const auto& attachment : renderPass.attachments)
        {
            auto aspect = ler::LerDevice::guessImageAspectFlags(attachment.format);
            if(aspect == vk::ImageAspectFlagBits::eColor)
                clearValues.emplace_back(vk::ClearColorValue(color));
            else
                clearValues.emplace_back(vk::ClearDepthStencilValue(1.0f, 0));
        }
        return clearValues;
    }

    RenderTargetPtr LerDevice::createRenderTarget(const vk::Extent2D& extent)
    {
        auto renderTarget = std::make_shared<RenderTarget>();
        renderTarget->renderPass = createSimpleRenderPass(vk::Format::eR8G8B8A8Unorm);
        renderTarget->frameBuffer = createFrameBuffer(renderTarget->renderPass, extent);
        renderTarget->clearValues = ler::LerDevice::clearRenderPass(renderTarget->renderPass, ler::Color::Gray);
        renderTarget->viewport = vk::Viewport(0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0, 1.0f);
        renderTarget->renderArea = vk::Rect2D(vk::Offset2D(), extent);
        renderTarget->extent = extent;
        return renderTarget;
    }

    void RenderTarget::beginRenderPass(vk::CommandBuffer& cmd)
    {
        vk::RenderPassBeginInfo beginInfo;
        beginInfo.setRenderPass(renderPass.handle.get());
        beginInfo.setFramebuffer(frameBuffer.handle.get());
        beginInfo.setRenderArea(renderArea);
        beginInfo.setClearValues(clearValues);
        cmd.beginRenderPass(beginInfo, vk::SubpassContents::eInline);
        cmd.setScissor(0, 1, &renderArea);
        cmd.setViewport(0, 1, &viewport);
    }

    uint32_t guessVertexInputBinding(const char* name)
    {
        for(size_t i = 0; i < c_VertexAttrMap.size(); ++i)
            if(c_VertexAttrMap[i].contains(name))
                return i;
        throw std::runtime_error("Vertex Input Attribute not reserved");
    }

    ShaderPtr LerDevice::createShader(const fs::path& path) const
    {
        auto shader = std::make_shared<Shader>();
        auto bytecode = FileSystemService::Get().readFile(path);
        vk::ShaderModuleCreateInfo shaderInfo;
        shaderInfo.setCodeSize(bytecode.size());
        shaderInfo.setPCode(reinterpret_cast<const uint32_t*>(bytecode.data()));
        shader->shaderModule = m_context.device.createShaderModuleUnique(shaderInfo);

        uint32_t count = 0;
        SpvReflectShaderModule module;
        SpvReflectResult result = spvReflectCreateShaderModule(bytecode.size(), bytecode.data(), &module);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);
        assert(module.generator == SPV_REFLECT_GENERATOR_KHRONOS_GLSLANG_REFERENCE_FRONT_END);

        shader->stageFlagBits = static_cast<vk::ShaderStageFlagBits>(module.shader_stage);
        log::debug("Reflect Shader Stage {}", vk::to_string(shader->stageFlagBits));

        // Input Variables
        result = spvReflectEnumerateInputVariables(&module, &count, nullptr);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        std::vector<SpvReflectInterfaceVariable*> inputs(count);
        result = spvReflectEnumerateInputVariables(&module, &count, inputs.data());
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        std::set<uint32_t> availableBinding;
        if (module.shader_stage == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT)
        {
            for(auto& in : inputs)
            {
                if(in->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN)
                    continue;

                uint32_t binding = guessVertexInputBinding(in->name);
                shader->attributeDesc.emplace_back(in->location, binding, static_cast<vk::Format>(in->format), 0);
                log::debug("location = {}, binding = {}, name = {}", in->location, binding, in->name);
                if(!availableBinding.contains(binding))
                {
                    shader->bindingDesc.emplace_back(binding, 0, vk::VertexInputRate::eVertex);
                    availableBinding.insert(binding);
                }
            }

            std::sort(shader->attributeDesc.begin(), shader->attributeDesc.end(),
                [](const VkVertexInputAttributeDescription& a, const VkVertexInputAttributeDescription& b) {
                    return a.location < b.location;
            });

            // Compute final offsets of each attribute, and total vertex stride.
            for (size_t i = 0; i < shader->attributeDesc.size(); ++i)
            {
                uint32_t format_size = formatSize(static_cast<VkFormat>(shader->attributeDesc[i].format));
                shader->attributeDesc[i].offset = shader->bindingDesc[i].stride;
                shader->bindingDesc[i].stride += format_size;
            }
        }

        shader->pvi = vk::PipelineVertexInputStateCreateInfo();
        shader->pvi.setVertexAttributeDescriptions(shader->attributeDesc);
        shader->pvi.setVertexBindingDescriptions(shader->bindingDesc);

        // Push Constants
        result = spvReflectEnumeratePushConstantBlocks(&module, &count, nullptr);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        std::vector<SpvReflectBlockVariable*> constants(count);
        result = spvReflectEnumeratePushConstantBlocks(&module, &count, constants.data());
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        for(auto& block : constants)
            shader->pushConstants.emplace_back(shader->stageFlagBits, block->offset, block->size);

        // Descriptor Set
        result = spvReflectEnumerateDescriptorSets(&module, &count, nullptr);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        std::vector<SpvReflectDescriptorSet*> sets(count);
        result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        for(auto& set : sets)
        {
            DescriptorSetLayoutData desc;
            desc.set_number = set->set;
            desc.bindings.resize(set->binding_count);
            for(size_t i = 0; i < set->binding_count; ++i)
            {
                auto& binding = desc.bindings[i];
                binding.binding = set->bindings[i]->binding;
                binding.descriptorCount = set->bindings[i]->count;
                binding.descriptorType = static_cast<vk::DescriptorType>(set->bindings[i]->descriptor_type);
                binding.stageFlags = shader->stageFlagBits;
                log::debug("set = {}, binding = {}, count = {:02}, type = {}", set->set, binding.binding, binding.descriptorCount, vk::to_string(binding.descriptorType));
            }
            shader->descriptorMap.insert({set->set, desc});
        }

        spvReflectDestroyShaderModule(&module);
        return shader;
    }

    void BasePipeline::reflectPipelineLayout(vk::Device device, const std::vector<ShaderPtr>& shaders)
    {
        // PIPELINE LAYOUT STATE
        auto layoutInfo = vk::PipelineLayoutCreateInfo();
        std::vector<vk::PushConstantRange> pushConstants;
        for(auto& shader : shaders)
            pushConstants.insert(pushConstants.end(), shader->pushConstants.begin(), shader->pushConstants.end());
        layoutInfo.setPushConstantRanges(pushConstants);

        // SHADER REFLECT
        std::set<uint32_t> sets;
        std::vector<vk::DescriptorPoolSize> descriptorPoolSizeInfo;
        std::multimap<uint32_t,DescriptorSetLayoutData> mergedDesc;
        for(auto& shader : shaders)
            mergedDesc.merge(shader->descriptorMap);

        for(auto& e : mergedDesc)
            sets.insert(e.first);

        std::vector<vk::DescriptorSetLayout> setLayouts;
        setLayouts.reserve(sets.size());
        for(auto& set : sets)
        {
            descriptorPoolSizeInfo.clear();
            auto it = descriptorAllocMap.emplace(set, DescriptorAllocator());
            auto& allocator = std::get<0>(it)->second;

            auto descriptorPoolInfo = vk::DescriptorPoolCreateInfo();
            auto descriptorLayoutInfo = vk::DescriptorSetLayoutCreateInfo();
            auto range = mergedDesc.equal_range(set);
            for (auto e = range.first; e != range.second; ++e)
                allocator.layoutBinding.insert(allocator.layoutBinding.end(), e->second.bindings.begin(), e->second.bindings.end());
            descriptorLayoutInfo.setBindings(allocator.layoutBinding);
            for(auto& b : allocator.layoutBinding)
                descriptorPoolSizeInfo.emplace_back(b.descriptorType, b.descriptorCount+2);
            descriptorPoolInfo.setPoolSizes(descriptorPoolSizeInfo);
            descriptorPoolInfo.setMaxSets(4);
            allocator.pool = device.createDescriptorPoolUnique(descriptorPoolInfo);
            allocator.layout = device.createDescriptorSetLayoutUnique(descriptorLayoutInfo);
            setLayouts.push_back(allocator.layout.get());
        }

        layoutInfo.setSetLayouts(setLayouts);
        pipelineLayout = device.createPipelineLayoutUnique(layoutInfo);
    }

    vk::DescriptorSet BasePipeline::createDescriptorSet(vk::Device& device, uint32_t set)
    {
        if(!descriptorAllocMap.contains(set))
            return {};

        vk::Result res;
        vk::DescriptorSet descriptorSet;
        const auto& allocator = descriptorAllocMap[set];
        vk::DescriptorSetAllocateInfo descriptorSetAllocInfo;
        descriptorSetAllocInfo.setDescriptorSetCount(1);
        descriptorSetAllocInfo.setDescriptorPool(allocator.pool.get());
        descriptorSetAllocInfo.setPSetLayouts(&allocator.layout.get());
        res = device.allocateDescriptorSets(&descriptorSetAllocInfo, &descriptorSet);
        assert(res == vk::Result::eSuccess);
        return descriptorSet;
    }

    void addShaderStage(std::vector<vk::PipelineShaderStageCreateInfo>& stages, const ShaderPtr& shader)
    {
        stages.emplace_back(
            vk::PipelineShaderStageCreateFlags(),
            shader->stageFlagBits,
            shader->shaderModule.get(),
            "main",
            nullptr
        );
    }

    PipelinePtr LerDevice::createGraphicsPipeline(const RenderPass& renderPass, const std::vector<ShaderPtr>& shaders, const PipelineInfo& info)
    {
        auto pipeline = std::make_shared<GraphicsPipeline>();
        std::vector<vk::PipelineShaderStageCreateInfo> pipelineShaderStages;
        for(auto& shader : shaders)
            addShaderStage(pipelineShaderStages, shader);

        // TOPOLOGY STATE
        vk::PipelineInputAssemblyStateCreateInfo pia(vk::PipelineInputAssemblyStateCreateFlags(), info.topology);

        // VIEWPORT STATE
        auto viewport = vk::Viewport(0, 0, static_cast<float>(info.extent.width), static_cast<float>(info.extent.height), 0, 1.0f);
        auto renderArea = vk::Rect2D(vk::Offset2D(), info.extent);

        vk::PipelineViewportStateCreateInfo pv(vk::PipelineViewportStateCreateFlagBits(), 1, &viewport, 1, &renderArea);

        // Multi Sampling STATE
        vk::PipelineMultisampleStateCreateInfo pm(vk::PipelineMultisampleStateCreateFlags(), info.sampleCount);

        // POLYGON STATE
        vk::PipelineRasterizationStateCreateInfo pr;
        pr.setDepthClampEnable(VK_TRUE);
        pr.setRasterizerDiscardEnable(VK_FALSE);
        pr.setPolygonMode(info.polygonMode);
        pr.setFrontFace(vk::FrontFace::eCounterClockwise);
        pr.setDepthBiasEnable(VK_FALSE);
        pr.setDepthBiasConstantFactor(0.f);
        pr.setDepthBiasClamp(0.f);
        pr.setDepthBiasSlopeFactor(0.f);
        pr.setLineWidth(info.lineWidth);

        // DEPTH & STENCIL STATE
        vk::PipelineDepthStencilStateCreateInfo pds;
        pds.setDepthTestEnable(VK_TRUE);
        pds.setDepthWriteEnable(info.writeDepth);
        pds.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
        pds.setDepthBoundsTestEnable(VK_FALSE);
        pds.setStencilTestEnable(VK_FALSE);
        pds.setFront(vk::StencilOpState());
        pds.setBack(vk::StencilOpState());
        pds.setMinDepthBounds(0.f);
        pds.setMaxDepthBounds(1.f);

        // BLEND STATE
        std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments;
        vk::PipelineColorBlendAttachmentState pcb;
        pcb.setBlendEnable(VK_FALSE); // false
        pcb.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha); //one //srcAlpha
        pcb.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha); //one //oneminussrcalpha
        pcb.setColorBlendOp(vk::BlendOp::eAdd);
        pcb.setSrcAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha); //one //oneminussrcalpha
        pcb.setDstAlphaBlendFactor(vk::BlendFactor::eZero); //zero
        pcb.setAlphaBlendOp(vk::BlendOp::eAdd);
        pcb.setColorWriteMask(
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA);

        for(auto& id : renderPass.subPass[info.subPass])
        {
            auto& attachment = renderPass.attachments[id];
            if(guessImageAspectFlags(attachment.format) == vk::ImageAspectFlagBits::eColor)
                colorBlendAttachments.push_back(pcb);
        }

        vk::PipelineColorBlendStateCreateInfo pbs;
        pbs.setLogicOpEnable(VK_FALSE);
        pbs.setLogicOp(vk::LogicOp::eClear);
        pbs.setAttachments(colorBlendAttachments);

        // DYNAMIC STATE
        std::vector<vk::DynamicState> dynamicStates =
        {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };

        vk::PipelineDynamicStateCreateInfo pdy(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

        // PIPELINE LAYOUT STATE
        auto layoutInfo = vk::PipelineLayoutCreateInfo();
        std::vector<vk::PushConstantRange> pushConstants;
        for(auto& shader : shaders)
            pushConstants.insert(pushConstants.end(), shader->pushConstants.begin(), shader->pushConstants.end());
        layoutInfo.setPushConstantRanges(pushConstants);

        // SHADER REFLECT
        vk::PipelineVertexInputStateCreateInfo pvi;
        for(auto& shader : shaders)
        {
            if(shader->stageFlagBits == vk::ShaderStageFlagBits::eVertex)
                pvi = shader->pvi;
            if(shader->stageFlagBits == vk::ShaderStageFlagBits::eFragment)
            {
                for(auto& e : shader->descriptorMap)
                {
                    for(auto& bind : e.second.bindings)
                        if(bind.descriptorCount == 0 && bind.descriptorType == vk::DescriptorType::eCombinedImageSampler)
                            bind.descriptorCount = info.textureCount;
                }
            }
        }

        pipeline->reflectPipelineLayout(m_context.device, shaders);

        auto pipelineInfo = vk::GraphicsPipelineCreateInfo();
        pipelineInfo.setRenderPass(renderPass.handle.get());
        pipelineInfo.setLayout(pipeline->pipelineLayout.get());
        pipelineInfo.setStages(pipelineShaderStages);
        pipelineInfo.setPVertexInputState(&pvi);
        pipelineInfo.setPInputAssemblyState(&pia);
        pipelineInfo.setPViewportState(&pv);
        pipelineInfo.setPRasterizationState(&pr);
        pipelineInfo.setPMultisampleState(&pm);
        pipelineInfo.setPDepthStencilState(&pds);
        pipelineInfo.setPColorBlendState(&pbs);
        pipelineInfo.setPDynamicState(&pdy);
        pipelineInfo.setSubpass(info.subPass);

        auto res = m_context.device.createGraphicsPipelineUnique(m_context.pipelineCache, pipelineInfo);
        assert(res.result == vk::Result::eSuccess);
        pipeline->handle = std::move(res.value);
        return pipeline;
    }

    PipelinePtr LerDevice::createComputePipeline(const ShaderPtr& shader)
    {
        auto pipeline = std::make_shared<ComputePipeline>();
        std::vector<vk::PipelineShaderStageCreateInfo> pipelineShaderStages;
        addShaderStage(pipelineShaderStages, shader);

        std::vector<ShaderPtr> shaders = { shader };
        pipeline->reflectPipelineLayout(m_context.device, shaders);

        auto pipelineInfo = vk::ComputePipelineCreateInfo();
        pipelineInfo.setStage(pipelineShaderStages.front());
        pipelineInfo.setLayout(pipeline->pipelineLayout.get());

        auto res = m_context.device.createComputePipelineUnique(m_context.pipelineCache, pipelineInfo);
        pipeline->bindPoint = vk::PipelineBindPoint::eCompute;
        assert(res.result == vk::Result::eSuccess);
        pipeline->handle = std::move(res.value);
        return pipeline;
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

    std::vector<char> LerDevice::loadBinaryFromFile(const fs::path& path)
    {
        std::vector<char> v;
        std::ifstream file(path, std::ios::binary);
        std::stringstream src;
        src << file.rdbuf();
        file.close();

        auto s = src.str();
        std::copy( s.begin(), s.end(), std::back_inserter(v));
        return v;
    }
}