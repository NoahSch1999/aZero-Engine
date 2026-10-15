#pragma once
#include "render_api/Buffer.hpp"
#include "asset/AssetData.hpp"
#include "render_api/Descriptor.hpp"
#include "asset/Material.hpp"
#include "asset/Mesh.hpp"
#include "misc/IndexFreelist.hpp"
#include "render_api/Sampler.hpp"

#include <unordered_map>

namespace aZero::Renderer
{
	

	struct Render_MeshletMesh
	{

		void Init(aZero::RenderAPI::Core& core, const aZero::Asset::MeshletMeshData& data)
		{
			uint32_t meshletOffset = 0, vertexOffset = 0, meshletBoundsOffset = 0;
			vertexOffset = data.Meshlets.size() * sizeof(data.Meshlets[0]);
			meshletBoundsOffset = vertexOffset + data.Vertices.size() * sizeof(data.Vertices[0]);
			uint64_t totalSize = meshletBoundsOffset + data.MeshletBounds.size() * sizeof(data.MeshletBounds[0]);
			RenderAPI::Buffer::Desc passBufferDesc{
				.Size = totalSize,
				.UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				.MemoryType = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO,
				.SeperateHeap = false
			};

			DataBuffer = RenderAPI::Buffer(core, passBufferDesc);
			DataBuffer.Upload(data.Meshlets, meshletOffset);
			DataBuffer.Upload(data.Vertices, vertexOffset);
			DataBuffer.Upload(data.MeshletBounds, meshletBoundsOffset);

			GpuData.MeshletPtr = DataBuffer.GetDeviceAddress() + meshletOffset;
			GpuData.VertexPtr = DataBuffer.GetDeviceAddress() + vertexOffset;
			GpuData.MeshletBoundPtr = DataBuffer.GetDeviceAddress() + meshletBoundsOffset;
		}

		RenderAPI::Buffer DataBuffer;
		aZero::Asset::MeshRenderRef GpuData;
	};

	struct Render_Texture
	{
		//void Init(const aZero::Asset::ImageData& imageData, RenderAPI::DescriptorSet& descriptorSet, RenderAPI::Sampler& sampler)
		//{
		//	RenderAPI::DescriptorHandle descriptorHandle{
		//		.Sampler = &sampler,
		//		.View = &DataImageView,
		//		.Layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
		//		.Index = 0
		//	};
		//	descriptorSet.UpdateDescriptor(descriptorHandle);
		//	//Descriptor = std::move(descriptor);
		//	//GpuData.DescriptorIndex = Descriptor.Index;
		//}

		RenderAPI::ImageView DataImageView;
		RenderAPI::Image DataImage;
		RenderAPI::DescriptorHandle Descriptor;
		aZero::Asset::TextureRenderRef GpuData;
	};

	class RenderAssetManager
	{
	public:
		RenderAssetManager() = default;
		RenderAssetManager(RenderAPI::Core& core)
		{
			RenderAPI::Buffer::Desc passBufferDesc{
				.Size = sizeof(aZero::Asset::PBRMaterialData) * 1000,
				.UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				.MemoryType = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO,
				.SeperateHeap = false
			};

			m_MaterialBuffer = RenderAPI::Buffer(core, passBufferDesc);
		}

		void UploadMesh(RenderAPI::Core& core, Asset::Mesh& mesh)
		{
			Render_MeshletMesh m;
			m.Init(core, mesh.GetCachedData());
			mesh.m_RenderRef = m.GpuData;
			m_Meshes.emplace(m.GpuData.MeshletPtr, std::move(m));
		}

		void UploadTexture(RenderAPI::Core& core,
			RenderAPI::CommandQueue& commandQueue, RenderAPI::CommandBuffer& commandBuffer,
			RenderAPI::DescriptorSet& descriptorSet,
			Asset::Texture& texture, 
			RenderAPI::Sampler& sampler)
		{
			Render_Texture tex;
			
			RenderAPI::Image::Desc imageDesc{
				.Format = texture.GetCachedData().Format,
				.Type = texture.GetCachedData().Type,
				.Usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT,
				.Width = texture.GetCachedData().BaseWidth, .Height = texture.GetCachedData().BaseHeight, .Depth = texture.GetCachedData().DepthOrArrayCount,
				.MipCount = (uint32_t)texture.GetCachedData().MipLevels.size()
			};

			tex.DataImage = RenderAPI::Image(core, imageDesc);

			RenderAPI::ImageView::Desc imageViewDesc{
				.Image = &tex.DataImage,
				.Type = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D,
				.ImageAspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
				.LayerStart = 0u,
				.LayerCount = 1u,
				.MipStart = 0,
				.MipCount = (uint32_t)texture.GetCachedData().MipLevels.size()
			};

			tex.DataImageView = RenderAPI::ImageView(core, imageViewDesc);
			
			tex.Descriptor = RenderAPI::DescriptorHandle {
				.Sampler = &sampler,
				.View = &tex.DataImageView,
				.Layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
				.Index = m_TextureIndexFreelist.New()
			};
			descriptorSet.UpdateDescriptor(tex.Descriptor);

			tex.GpuData.DescriptorIndex = tex.Descriptor.Index;

			texture.m_RenderRef = tex.GpuData;

			RenderAPI::Buffer::Desc imgSrcBufferDesc{
			  .Size = (uint32_t)texture.GetCachedData().Data.size(),
			  .UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			  .MemoryType = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY,
			  .SeperateHeap = false
			};

			RenderAPI::Buffer imgSrcBuffer(core, imgSrcBufferDesc);

			imgSrcBuffer.Upload(texture.GetCachedData().Data.data(), texture.GetCachedData().Data.size(), 0);

			std::vector<uint32_t> mipOffsets;
			for (auto& ele : texture.GetCachedData().MipLevels) {
				mipOffsets.push_back(ele.Offset);
			}

			RenderAPI::BufferToImageCopyDesc uploadDesc{
			   .BaseWidth = texture.GetCachedData().BaseWidth,
			   .BaseHeight = texture.GetCachedData().BaseHeight,
			   .DepthOrArrayCount = texture.GetCachedData().DepthOrArrayCount,
			   .MipOffsets = mipOffsets
			};

			tex.DataImage.Upload(core, commandQueue, commandBuffer, imgSrcBuffer, uploadDesc);

			m_Textures.emplace(tex.GpuData.DescriptorIndex, std::move(tex));
		}

		void UploadMaterial(RenderAPI::Core& core, Asset::Material& material)
		{
			material.m_RenderRef = m_MaterialIndexFreelist.New();

			aZero::Asset::PBRMaterialData matData{
				.AlbedoTexture = material.GetAlbedoPtr()->GetRenderRef().DescriptorIndex
			};

			m_MaterialBuffer.Upload(matData, material.m_RenderRef * sizeof(matData));
		}

	private:
		std::unordered_map<VkDeviceAddress, Render_MeshletMesh> m_Meshes;
		std::unordered_map<uint32_t, Render_Texture> m_Textures;
		aZero::DS::IndexFreelist<decltype(RenderAPI::DescriptorHandle::Index)> m_TextureIndexFreelist;
		RenderAPI::Buffer m_MaterialBuffer;
		aZero::DS::IndexFreelist<uint32_t> m_MaterialIndexFreelist;
	};
}