#pragma once
#include "asset/Material.hpp"
#include "misc/IndexFreelist.hpp"
#include "FrameAllocator.hpp"

namespace aZero::Renderer
{
	class GpuMaterialSystem
	{
	public:
		GpuMaterialSystem() = default;
		GpuMaterialSystem(RenderAPI::Core& core, ResourceRecycler& recycler)
		{
			uint64_t size = 1000 * sizeof(Asset::PBRMaterialData);
			const RenderAPI::Buffer::Desc desc{
				.Size = size,
				.UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				.MemoryType = RenderAPI::EMemoryType::VRAM,
				.SeperateHeap = true,
				.OptResourceRecycler = recycler
			};
			m_MaterialBuffer.Init(core, desc);
		}

		~GpuMaterialSystem()
		{
		}

		GpuMaterialSystem(GpuMaterialSystem&& other) noexcept
		{
			*this = std::move(other);
		}

		GpuMaterialSystem& operator=(GpuMaterialSystem&& other) noexcept
		{
			std::swap(m_MaterialBuffer, other.m_MaterialBuffer);
			std::swap(m_IndexFreelist, other.m_IndexFreelist);
			return *this;
		}

		void Update(Asset::Material& material, FrameAllocator& frameAllocator)
		{
			if (!material.m_RenderRef.IsValid())
			{
				uint32_t newIndex = m_IndexFreelist.New();
				if (newIndex * sizeof(Asset::PBRMaterialData) >= m_MaterialBuffer.GetDesc().Size)
				{
					// todo Handle if buffer is full

					//
				}

				material.m_RenderRef.MaterialIndex = newIndex;
			}

			uint32_t offset = material.m_RenderRef.MaterialIndex * sizeof(Asset::PBRMaterialData);
			auto data = material.GetFormat_PBR_GPU();
			frameAllocator.Allocate(&data, sizeof(data), m_MaterialBuffer, offset);
		}

		void Destroy(Asset::Material& material)
		{
			if (!material.m_RenderRef.IsValid())
			{
				return;
			}
			m_IndexFreelist.Recycle(material.m_RenderRef.MaterialIndex); // We can instantly recycle the index (contrary to descriptor indices in GpuTextureSystem) since the material data is uploaded via the command queue and not directly from the cpu-side.
			material.m_RenderRef = Asset::MaterialRenderRef();
		}

	private:
		RenderAPI::Buffer m_MaterialBuffer;
		aZero::DS::IndexFreelist<uint32_t> m_IndexFreelist;
	};
}