#pragma once
#include "asset/Texture.hpp"
#include "render_api/Descriptor.hpp"
#include "misc/IndexFreelist.hpp"

namespace aZero::Renderer
{
	class GpuTextureSystem
	{
	public:
		GpuTextureSystem(RenderAPI::Core& core, uint32_t framesInFlight)
		{
            RenderAPI::DescriptorSet::Desc setDesc{ .Type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .DescriptorCount = 1000, .ShaderStages = VkShaderStageFlagBits::VK_SHADER_STAGE_ALL };
            RenderAPI::DescriptorPool::Desc descriptorPoolDesc{
                .MaxSets = 1,
                .Sets {setDesc}
            };
            m_DescriptorPool.Init(core, descriptorPoolDesc);
            m_DescriptorSet = m_DescriptorPool.CreateDescriptorSet(setDesc);

            m_DeferredDestroyedHandles.resize(framesInFlight);
		}

        ~GpuTextureSystem()
        {
        }

        GpuTextureSystem(GpuTextureSystem&& other) noexcept
        {
            *this = std::move(other);
        }

        GpuTextureSystem& operator=(GpuTextureSystem&& other) noexcept
        {
            std::swap(m_GpuTextures, other.m_GpuTextures);
            std::swap(m_DeferredDestroyedHandles, other.m_DeferredDestroyedHandles);
            std::swap(m_IndexFreelist, other.m_IndexFreelist);
            std::swap(m_DescriptorPool, other.m_DescriptorPool);
            std::swap(m_DescriptorSet, other.m_DescriptorSet);
            std::swap(m_FrameIndex, other.m_FrameIndex);
            return *this;
        }

        void NextFrame(uint32_t frameIndex)
        {
            m_FrameIndex = frameIndex;

            // ::NextFrame() is called after the renderer has done a cpu-wait on the frame fence for the "frameIndex" frame. This lets us reuse indices of textures that has been destroyed last time "frameIndex" happened since we know that they aren't in use anymore :)
            for (const auto& descriptorHandle : m_DeferredDestroyedHandles[m_FrameIndex])
            {
                m_IndexFreelist.Recycle(descriptorHandle.Index);
            }
            m_DeferredDestroyedHandles[m_FrameIndex].clear();
        }

        void Update(Asset::Texture& texture, RenderAPI::Core& core, ResourceRecycler& recycler, const RenderAPI::CommandBuffer& commandBuffer)
        {
            if (texture.m_RenderRef.IsValid())
            {
                this->Destroy(texture);
            }

            texture.m_RenderRef.DescriptorIndex = m_IndexFreelist.New();

            GpuTexture gpuTexture;

            const auto& cachedData = texture.GetCachedData();
            RenderAPI::Image::Desc imageDesc{
                .Format = cachedData.Format,
                .Type = VkImageType::VK_IMAGE_TYPE_2D,
                .Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .Width = (uint32_t)cachedData.BaseWidth,
                .Height = (uint32_t)cachedData.BaseHeight,
                .Depth = 1,
                .MipCount = (uint32_t)cachedData.MipLevels.size(),
                .ArrayLayers = 1,
                .OptResourceRecycler = recycler
            };
            gpuTexture.Image.Init(core, imageDesc);

            RenderAPI::ImageView::Desc imageViewDesc{
                .Image = &gpuTexture.Image,
                .Type = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D,
                .ImageAspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
                .LayerStart = 0u,
                .LayerCount = 1u,
                .MipStart = 0,
                .MipCount = (uint32_t)cachedData.MipLevels.size()
            };
            gpuTexture.View.Init(core, imageViewDesc);

            gpuTexture.Sampler.Init(core, {}); // todo Handle per-texture sampler

            RenderAPI::DescriptorHandle descriptorHandle{
                .Sampler = &gpuTexture.Sampler,
                .View = &gpuTexture.View,
                .Layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                .Index = texture.m_RenderRef.DescriptorIndex
            };

            m_DescriptorSet.UpdateDescriptor(descriptorHandle);

            gpuTexture.Descriptor = descriptorHandle;

            RenderAPI::Buffer::Desc stagingBufferDesc{
                .Size = (uint64_t)cachedData.Data.size(),
                .UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .MemoryType = RenderAPI::EMemoryType::SYSRAM,
                .SeperateHeap = true,
                .OptResourceRecycler = recycler
            };
            RenderAPI::Buffer stagingBuffer(core, stagingBufferDesc);
            stagingBuffer.Upload(cachedData.Data, 0);
            
            std::vector<uint32_t> mipOffsets;
            for (auto& ele : cachedData.MipLevels) {
                mipOffsets.push_back(ele.Offset);
            }

            RenderAPI::BufferToImageCopyDesc copyDesc {
               .BaseWidth = cachedData.BaseWidth,
               .BaseHeight = cachedData.BaseHeight,
               .DepthOrArrayCount = cachedData.DepthOrArrayCount,
               .MipOffsets = mipOffsets
            };

            commandBuffer.Copy(copyDesc, stagingBuffer, gpuTexture.Image);

            VkImageSubresourceRange subresourceRange{
                .aspectMask = imageViewDesc.ImageAspect,
                .baseMipLevel = 0,
                .levelCount = (uint32_t)cachedData.MipLevels.size(),
                .baseArrayLayer = 0,
                .layerCount = 1
            };
            VkImageMemoryBarrier2 barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,

                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT |
                                 VK_ACCESS_2_MEMORY_WRITE_BIT,

                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,

                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

                .image = gpuTexture.Image.Get(),
                .subresourceRange = subresourceRange
            };

            commandBuffer.Barrier({}, { barrier }, {});

            m_GpuTextures[texture.m_RenderRef.DescriptorIndex] = std::move(gpuTexture);
        }

        void Destroy(Asset::Texture& texture)
        {
            if (!texture.GetRenderRef().IsValid())
            {
                return;
            }

            m_DeferredDestroyedHandles[m_FrameIndex].push_back(m_GpuTextures.at(texture.m_RenderRef.DescriptorIndex).Descriptor); // Saves the descriptor index so that it can't be reused until the resource isn't in use anymore.
            m_GpuTextures.erase(texture.m_RenderRef.DescriptorIndex); // Recycle the actual gpu resources via RAII :)
            texture.m_RenderRef = Asset::TextureRenderRef();
        }

        const RenderAPI::DescriptorSet& GetDescriptorSet() const { return m_DescriptorSet; }

	private:
        struct GpuTexture
        {
            RenderAPI::Image Image;
            RenderAPI::ImageView View;
            RenderAPI::Sampler Sampler;
            RenderAPI::DescriptorHandle Descriptor;
        };

        std::unordered_map<uint32_t, GpuTexture> m_GpuTextures;
        std::vector<std::vector<RenderAPI::DescriptorHandle>> m_DeferredDestroyedHandles; // Deferres descriptor index reusage until next time the same frameindex happens again
        aZero::DS::IndexFreelist<uint32_t> m_IndexFreelist;
        RenderAPI::DescriptorPool m_DescriptorPool;
        RenderAPI::DescriptorSet m_DescriptorSet; // This is used with renderpasses
        uint32_t m_FrameIndex;
	};
}