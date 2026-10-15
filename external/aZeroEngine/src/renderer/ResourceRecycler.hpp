#pragma once
#include "render_api/Core.hpp"
#include "misc/NonMoveCopy.hpp"
#include <stdint.h>

namespace aZero::Renderer
{
	struct RecycledBuffer
	{
		VkBuffer Buffer;
		VmaAllocation Allocation;
	};

	struct RecycledImage
	{
		VkImage Image;
		VmaAllocation Allocation;
	};

	class ResourceRecycler : public aZero::NonCopyable
	{
	public:

		ResourceRecycler() = default;
		ResourceRecycler(RenderAPI::Core& core, uint32_t framesInFlight, uint32_t startIndex)
		{
			m_diCore = &core;
			m_Buffers.resize(framesInFlight);
			m_Images.resize(framesInFlight);
			m_FrameIndex = startIndex;
		}

		ResourceRecycler(ResourceRecycler&& other) noexcept
		{
			*this = std::move(other);
		}

		ResourceRecycler& operator=(ResourceRecycler&& other) noexcept
		{
			std::swap(m_Buffers, other.m_Buffers);
			std::swap(m_Images, other.m_Images);
			std::swap(m_diCore, other.m_diCore);
			std::swap(m_FrameIndex, other.m_FrameIndex);
			return *this;
		}

		void NextFrame(uint32_t frameIndex)
		{
			this->Clear(frameIndex);

			m_FrameIndex = frameIndex;
		}
		
		void Recycle(VkBuffer buffer, VmaAllocation allocation)
		{
			m_Buffers[m_FrameIndex].emplace_back(buffer, allocation);
		}

		void Recycle(VkImage image, VmaAllocation allocation)
		{
			m_Images[m_FrameIndex].emplace_back(image, allocation);
		}

		void Clear(uint32_t frameIndex)
		{
			for (auto& buffer : m_Buffers[frameIndex])
			{
				vmaDestroyBuffer(m_diCore->GetVmaAllocator(), buffer.Buffer, buffer.Allocation);
			}
			m_Buffers[frameIndex].clear();

			for (auto& image : m_Images[frameIndex])
			{
				vmaDestroyImage(m_diCore->GetVmaAllocator(), image.Image, image.Allocation);
			}
			m_Images[frameIndex].clear();
		}

	private:
		std::vector<std::vector<RecycledBuffer>> m_Buffers;
		std::vector<std::vector<RecycledImage>> m_Images;
		aZero::RenderAPI::Core* m_diCore = nullptr;
		std::uint32_t m_FrameIndex;
	};
}