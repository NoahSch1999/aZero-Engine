#pragma once
#include <unordered_set>
#include <unordered_map>
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
        static inline const char* s_Null_TextureStr = "Null_Texture";
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

            m_DeferredDestroyedIndices.resize(framesInFlight);
            m_DeferredDestroyedViews.resize(framesInFlight);
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
            std::swap(m_DeferredDestroyedIndices, other.m_DeferredDestroyedIndices);
            std::swap(m_DeferredDestroyedViews, other.m_DeferredDestroyedViews);
            std::swap(m_IndexFreelist, other.m_IndexFreelist);
            std::swap(m_DescriptorPool, other.m_DescriptorPool);
            std::swap(m_DescriptorSet, other.m_DescriptorSet);
            std::swap(m_FrameIndex, other.m_FrameIndex);

            return *this;
        }

        std::optional<std::reference_wrapper<Asset::Texture>> CreateTexture(const std::string& name, const Asset::ImageData& data)
        {
            if (m_Textures.contains(name) || !data.HasValidData())
            {
                return std::nullopt;
            }

            GpuTexture gpuTexture;
            gpuTexture.Texture.m_DescriptorIndex = m_IndexFreelist.New();
            gpuTexture.Texture.Name = name;
            gpuTexture.Texture.m_diTextureSystem = this;

            auto [it, inserted] = m_Textures.emplace(name, std::move(gpuTexture));
            it->second.Texture.SetData(data);
            return it->second.Texture;
        }

        void Destroy(const Asset::Texture& texture)
        {
            if (auto iter = m_Textures.find(texture.Name); iter != m_Textures.end())
            {
                // todo Handle all references to the texture.
                //      Once descriptor heaps are used we can simply copy a fallback-texture-descriptor to the same slot via a commandbuffer+barrier.

                m_DirtyTextures.erase(&texture);
                m_DeferredDestroyedIndices[m_FrameIndex].push_back(iter->second.Descriptor.Index); // Saves the descriptor index so that it can't be reused until the resource isn't in use anymore.
                m_DeferredDestroyedViews[m_FrameIndex].push_back(std::move(iter->second.View));
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

        void SetTextureDirty(const Asset::Texture& texture)
        {
            m_DirtyTextures.emplace(&texture);
        }

	private:

        void NextFrame(uint32_t frameIndex, RenderAPI::CommandBuffer& diFrameCommandBuffer)
        {
            m_diFrameCommandBuffer = &diFrameCommandBuffer;
            m_FrameIndex = frameIndex;

            // ::NextFrame() is called after the renderer has done a cpu-wait on the frame fence for the "frameIndex" frame. This lets us reuse indices of textures that has been destroyed last time "frameIndex" happened since we know that they aren't in use anymore :)
            for (const auto& descriptorIndex : m_DeferredDestroyedIndices[m_FrameIndex])
            {
                m_IndexFreelist.Recycle(descriptorIndex);
            }
            m_DeferredDestroyedIndices[m_FrameIndex].clear();
            m_DeferredDestroyedViews[m_FrameIndex].clear();
        }

        void UpdateDirtyTextures()
        {
            bool hasUpdated = false;
            for (const auto& texture : m_DirtyTextures)
            {
                if (!hasUpdated)
                {
                    m_diCore->FlushGPU();
                    hasUpdated = true;
                }

                if (auto iter = m_Textures.find(texture->Name); iter != m_Textures.end())
                {
                    this->UploadTexture(texture->m_Data, iter->second);
                }
            }

            m_DirtyTextures.clear();
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
            gpuTexture.Image = RenderAPI::Image(*m_diCore, imageDesc);

            RenderAPI::ImageView::Desc imageViewDesc{
                .Image = &gpuTexture.Image,
                .Type = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D,
                .ImageAspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
                .LayerStart = 0u,
                .LayerCount = 1u,
                .MipStart = 0,
                .MipCount = (uint32_t)cachedData.MipLevels.size()
            };
            gpuTexture.View = RenderAPI::ImageView(*m_diCore, imageViewDesc);

            gpuTexture.Sampler = RenderAPI::Sampler(*m_diCore, {}); // todo Handle per-texture sampler

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

            RenderAPI::Image::TransitionDesc trDesc{
                .DstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                .DstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .NewLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .SubresourceRange = {
                    .aspectMask = imageViewDesc.ImageAspect,
                    .baseMipLevel = 0,
                    .levelCount = (uint32_t)cachedData.MipLevels.size(),
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            };
            gpuTexture.Image.Transition(trDesc);
            m_diFrameCommandBuffer->Barrier({}, { gpuTexture.Image.GetTransition() }, {});

            m_diFrameCommandBuffer->Copy(copyDesc, stagingBuffer, gpuTexture.Image);

            trDesc.DstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
            trDesc.DstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            trDesc.NewLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
            gpuTexture.Image.Transition(trDesc);
            m_diFrameCommandBuffer->Barrier({}, { gpuTexture.Image.GetTransition() }, {});
        }

        std::unordered_set<const Asset::Texture*> m_DirtyTextures;
        RenderAPI::CommandBuffer* m_diFrameCommandBuffer = nullptr;
        Renderer::ResourceRecycler* m_diRecycler = nullptr;
        RenderAPI::Core* m_diCore = nullptr;
        std::unordered_map<std::string, GpuTexture> m_Textures;
		std::vector<std::vector<RenderAPI::DescriptorIndex>> m_DeferredDestroyedIndices; // Deferres descriptor index reusage until next time the same frameindex happens again
		std::vector<std::vector<RenderAPI::ImageView>> m_DeferredDestroyedViews;
        aZero::DS::IndexFreelist<uint32_t> m_IndexFreelist;
        RenderAPI::DescriptorPool m_DescriptorPool;
        RenderAPI::DescriptorSet m_DescriptorSet; // This is used with renderpasses
        uint32_t m_FrameIndex;
	};
}