#pragma once
#include "asset/texture/Texture.hpp"
#include "render_api/Descriptor.hpp"
#include "misc/IndexFreelist.hpp"

namespace aZero::Renderer { class Renderer; }

namespace aZero::System
{
	class TextureSystem
	{
        friend class aZero::Renderer::Renderer;

        struct GpuTexture
        {
            RenderAPI::Image Image;
            RenderAPI::ImageView View;
            RenderAPI::Sampler Sampler;
            RenderAPI::DescriptorHandle Descriptor;
            Asset::Texture Texture;
        };
	public:
        TextureSystem() = default;
        TextureSystem(RenderAPI::Core& core, Renderer::ResourceRecycler& recycler, uint32_t framesInFlight)
            :m_diCore(&core), m_diRecycler(&recycler)
		{
            RenderAPI::DescriptorSet::Desc setDesc{ .Type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .MaxDescriptorCount = core.GetVariadicDescriptorCount(), .DescriptorCount = 1000, .ShaderStages = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT};
            RenderAPI::DescriptorPool::Desc descriptorPoolDesc{
                .MaxSets = 1,
                .Sets {setDesc}
            };
            m_DescriptorPool.Init(core, descriptorPoolDesc);
            m_DescriptorSet = m_DescriptorPool.CreateDescriptorSet(setDesc);

            m_DeferredDestroyedHandles.resize(framesInFlight);
		}

        ~TextureSystem()
        {
        }

        TextureSystem(TextureSystem&& other) noexcept
        {
            *this = std::move(other);
        }

        TextureSystem& operator=(TextureSystem&& other) noexcept
        {
            std::swap(m_diFrameCommandBuffer, other.m_diFrameCommandBuffer);
            std::swap(m_diRecycler, other.m_diRecycler);
            std::swap(m_diCore, other.m_diCore);
            std::swap(m_Textures, other.m_Textures);
            std::swap(m_DeferredDestroyedHandles, other.m_DeferredDestroyedHandles);
            std::swap(m_IndexFreelist, other.m_IndexFreelist);
            std::swap(m_DescriptorPool, other.m_DescriptorPool);
            std::swap(m_DescriptorSet, other.m_DescriptorSet);
            std::swap(m_FrameIndex, other.m_FrameIndex);

            return *this;
        }

        // todo Call this for each dirty texture
        std::optional<std::reference_wrapper<Asset::Texture>> CreateTexture(const std::string& name, const Asset::ImageData& data)
        {
            if (m_Textures.contains(name) || !data.HasValidData())
            {
                return std::nullopt;
            }

            GpuTexture gpuTexture;
            // Note! Ideally we want this to be a stable index across updates to avoid having to update materialinstances that reference it... 
            //          However, since we dont use descriptor heaps yet this isnt possible unless we defere the update 3 frames until the texture isnt in use anymore...
            gpuTexture.Texture.m_DescriptorIndex = m_IndexFreelist.New();
            gpuTexture.Texture.Name = name;
            gpuTexture.Texture.m_Data = data;
            this->UploadTexture(data, gpuTexture);
            gpuTexture.Texture.m_Data = data;
            gpuTexture.Texture.SetNotDirty(); // To avoid auto-uploading the asset

            auto [it, inserted] = m_Textures.emplace(name, std::move(gpuTexture));
            return it->second.Texture;
        }

        void Destroy(const Asset::Texture& texture)
        {
            if (auto iter = m_Textures.find(texture.Name); iter != m_Textures.end())
            {
                // todo Handle all references to the texture

                m_DeferredDestroyedHandles[m_FrameIndex].push_back(iter->second.Descriptor); // Saves the descriptor index so that it can't be reused until the resource isn't in use anymore.
                m_Textures.erase(iter); // Recycle the actual gpu resources via RAII :)

                return;
            }

        }

        std::optional<std::reference_wrapper<Asset::Texture>> GetTexture(const std::string& name)
        {
            if (auto iter = m_Textures.find(name); iter != m_Textures.end())
            {
                return iter->second.Texture;
            }
            return std::nullopt;
        }

	private:

        void NextFrame(uint32_t frameIndex, RenderAPI::CommandBuffer& diFrameCommandBuffer)
        {
            m_diFrameCommandBuffer = &diFrameCommandBuffer;
            m_FrameIndex = frameIndex;

            // ::NextFrame() is called after the renderer has done a cpu-wait on the frame fence for the "frameIndex" frame. This lets us reuse indices of textures that has been destroyed last time "frameIndex" happened since we know that they aren't in use anymore :)
            for (const auto& descriptorHandle : m_DeferredDestroyedHandles[m_FrameIndex])
            {
                m_IndexFreelist.Recycle(descriptorHandle.Index);
            }
            m_DeferredDestroyedHandles[m_FrameIndex].clear();
        }

        void UpdateDirtyTextures()
        {
            bool hasUpdated = false;
            for (auto& [name, gpuTexture] : m_Textures) // Horrible, but good for now...
            {
                if (gpuTexture.Texture.IsDirty())
                {
                    if (!hasUpdated)
                    {
                        // todo Since we want to make descriptorindices stable across updates we flush to be able to update the descriptors... Fix this once descriptor heaps are used...
                        m_diCore->FlushGPU();
                        hasUpdated = true;
                    }
                    gpuTexture.Texture.SetNotDirty();
                    this->UploadTexture(gpuTexture.Texture.m_Data, gpuTexture);
                }
            }
        }

        void UploadTexture(const Asset::ImageData& data, GpuTexture& gpuTexture)
        {
            const auto& cachedData = data;

            RenderAPI::Image::Desc imageDesc{
                .Format = cachedData.Format,
                .Type = data.Type,
                .Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .Width = (uint32_t)cachedData.BaseWidth,
                .Height = (uint32_t)cachedData.BaseHeight,
                .Depth = (uint32_t)cachedData.DepthOrArrayCount,
                .MipCount = (uint32_t)cachedData.MipLevels.size(),
                .ArrayLayers = (uint32_t)cachedData.FaceCount,
                .Name = gpuTexture.Texture.Name,
                .OptResourceRecycler = *m_diRecycler
            };
            gpuTexture.Image.Init(*m_diCore, imageDesc);

            RenderAPI::ImageView::Desc imageViewDesc{
                .Image = &gpuTexture.Image,
                .Type = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D,
                .ImageAspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
                .LayerStart = 0u,
                .LayerCount = 1u,
                .MipStart = 0,
                .MipCount = (uint32_t)cachedData.MipLevels.size()
            };
            gpuTexture.View.Init(*m_diCore, imageViewDesc);

            gpuTexture.Sampler.Init(*m_diCore, {}); // todo Handle per-texture sampler

            RenderAPI::DescriptorHandle descriptorHandle{
                .Sampler = &gpuTexture.Sampler,
                .View = &gpuTexture.View,
                .Layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                .Index = gpuTexture.Texture.m_DescriptorIndex
            };

            m_DescriptorSet.UpdateDescriptor(descriptorHandle);

            gpuTexture.Descriptor = descriptorHandle;

            RenderAPI::Buffer::Desc stagingBufferDesc{
                .Size = (uint64_t)cachedData.Data.size(),
                .UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                .MemoryType = RenderAPI::EMemoryType::SYSRAM,
                .SeperateHeap = true,
                .OptResourceRecycler = *m_diRecycler
            };
            RenderAPI::Buffer stagingBuffer(*m_diCore, stagingBufferDesc);
            stagingBuffer.Upload(cachedData.Data, 0);

            std::vector<uint32_t> mipOffsets;
            for (auto& ele : cachedData.MipLevels) {
                mipOffsets.push_back(ele.Offset);
            }

            RenderAPI::BufferToImageCopyDesc copyDesc{
               .BaseWidth = cachedData.BaseWidth,
               .BaseHeight = cachedData.BaseHeight,
               .DepthOrArrayCount = cachedData.DepthOrArrayCount,
               .MipOffsets = mipOffsets
            };

            VkImageSubresourceRange subresourceRange{
                .aspectMask = imageViewDesc.ImageAspect,
                .baseMipLevel = 0,
                .levelCount = (uint32_t)cachedData.MipLevels.size(),
                .baseArrayLayer = 0,
                .layerCount = 1
            };

            VkImageMemoryBarrier2 barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask = VK_ACCESS_2_NONE,

                .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,

                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,

                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

                .image = gpuTexture.Image.Get(),
                .subresourceRange = subresourceRange
            };
            m_diFrameCommandBuffer->Barrier({}, { barrier }, {});

            m_diFrameCommandBuffer->Copy(copyDesc, stagingBuffer, gpuTexture.Image);

            barrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,

                .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,

                .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT |
                                 VK_ACCESS_2_MEMORY_WRITE_BIT,

                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,

                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

                .image = gpuTexture.Image.Get(),
                .subresourceRange = subresourceRange
            };
            m_diFrameCommandBuffer->Barrier({}, { barrier }, {});
        }

        RenderAPI::CommandBuffer* m_diFrameCommandBuffer = nullptr;
        Renderer::ResourceRecycler* m_diRecycler = nullptr;
        RenderAPI::Core* m_diCore = nullptr;
        std::unordered_map<std::string, GpuTexture> m_Textures;
		std::vector<std::vector<RenderAPI::DescriptorHandle>> m_DeferredDestroyedHandles; // Deferres descriptor index reusage until next time the same frameindex happens again
        aZero::DS::IndexFreelist<uint32_t> m_IndexFreelist;
        RenderAPI::DescriptorPool m_DescriptorPool;
        RenderAPI::DescriptorSet m_DescriptorSet; // This is used with renderpasses
        uint32_t m_FrameIndex;
	};
}