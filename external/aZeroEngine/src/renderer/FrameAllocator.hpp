#pragma once
#include <unordered_map>
#include "render_api/Buffer.hpp"
#include "render_api/CommandPool.hpp"
#include "misc/LinearAllocator.hpp"

namespace aZero::Renderer
{
	/// <summary>
	/// Scratch allocator that has a linear allocator staging buffer.
	/// Allocations first copy to the sys-ram buffer and then caches information for a copy-command from the staging buffer to the destination buffer.
	/// </summary>
	class FrameAllocator
	{
		friend class FrameContext;

		struct AllocationInfo
		{
			std::vector<VkBufferCopy> CopyRegions;
		};
	public:

		FrameAllocator() = default;
		FrameAllocator(uint32_t initialMemorySize, RenderAPI::Core& core, aZero::Renderer::ResourceRecycler& recycler)
		{
			const RenderAPI::Buffer::Desc desc {
				.Size = initialMemorySize,
				.UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				.MemoryType = RenderAPI::EMemoryType::SYSRAM,
				.SeperateHeap = false,
				.OptResourceRecycler = recycler
			};
			if (m_StagingBuffer.Init(core, desc))
			{
				m_Allocator = aZero::LinearAllocator<>(reinterpret_cast<std::byte*>(m_StagingBuffer.GetMappedPtr()), initialMemorySize);
			}

			m_diCore = &core;
		}

		FrameAllocator(FrameAllocator&& other) noexcept
		{
			*this = std::move(other);
		}

		FrameAllocator& operator=(FrameAllocator&& other) noexcept
		{
			std::swap(m_StagingBuffer, other.m_StagingBuffer);
			std::swap(m_Allocator, other.m_Allocator);
			std::swap(m_diCore, other.m_diCore);
			std::swap(m_Allocations, other.m_Allocations);
			return *this;
		}

		void Allocate(void* data, uint32_t size, const RenderAPI::Buffer& dstBuffer, uint32_t dstOffset)
		{
			std::byte* ptr = m_Allocator.Allocate(size);
			memcpy(ptr, data, size);
			uint32_t offset = m_Allocator.GetOffset() - size;

			VkBufferCopy copyRegion{
				.srcOffset = (VkDeviceSize)offset,
				.dstOffset = (VkDeviceSize)dstOffset,
				.size = (VkDeviceSize)size
			};
			m_Allocations[dstBuffer.Get()].CopyRegions.push_back(copyRegion);
		}

		template<typename T> requires std::is_trivially_copyable_v<T>
		void Allocate(const std::vector<T>& data, const RenderAPI::Buffer& dstBuffer, uint32_t dstOffset)
		{
			const uint32_t size = data.size() * sizeof(T);
			std::byte* ptr = m_Allocator.Allocate(size);
			memcpy(ptr, data.data(), size);
			uint32_t offset = m_Allocator.GetOffset() - size;

			VkBufferCopy copyRegion{
				.srcOffset = (VkDeviceSize)offset,
				.dstOffset = (VkDeviceSize)dstOffset,
				.size = (VkDeviceSize)size
			};
			m_Allocations[dstBuffer.Get()].CopyRegions.push_back(copyRegion);
		}

		void Record(RenderAPI::CommandBuffer& cmdBuffer, bool clear = true)
		{
			VkBufferMemoryBarrier2 stagingBufferBarrier = {
					.sType = VkStructureType::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
					.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
					.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
					.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
					.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
					.srcQueueFamilyIndex = m_diCore->GetIndexForQueueType(RenderAPI::EQueueType::Graphics),
					.dstQueueFamilyIndex = m_diCore->GetIndexForQueueType(RenderAPI::EQueueType::Graphics),
					.buffer = m_StagingBuffer.Get(),
					.offset = 0,
					.size = m_StagingBuffer.GetDesc().Size
			};
			cmdBuffer.Barrier({}, {}, { stagingBufferBarrier });

			for (const auto& [buffer, allocationInfo] : m_Allocations)
			{
				// Find the min and max copy range for the buffer to minimize sync on data that isnt touched. If the min and max are far apart its no big diff, but if they are compact it can be good! :)
				VkDeviceSize barrierSpanStart = 0, barrierSpanEnd = 0;
				if (!allocationInfo.CopyRegions.empty())
				{
					barrierSpanStart = allocationInfo.CopyRegions[0].dstOffset;
				}
				for (const auto& copyRegion : allocationInfo.CopyRegions)
				{
					if (copyRegion.dstOffset + copyRegion.size > barrierSpanEnd)
					{
						barrierSpanEnd = copyRegion.dstOffset + copyRegion.size;
					}

					if (copyRegion.dstOffset < barrierSpanStart)
					{
						barrierSpanStart = copyRegion.dstOffset;
					}
				}
				//

				// Waiting on all commands for the buffer that read/write WHEN any copy write commands are supposed to run on it.
				VkBufferMemoryBarrier2 preCopyBarrier = {
					.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,

					.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
					.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT |
									 VK_ACCESS_2_MEMORY_WRITE_BIT,

					.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
					.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,

					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

					.buffer = buffer,
					.offset = barrierSpanStart,
					.size = barrierSpanEnd - barrierSpanStart
				};

				cmdBuffer.Barrier({}, {}, { preCopyBarrier });
				
				cmdBuffer.Copy(m_StagingBuffer.Get(), buffer, allocationInfo.CopyRegions);

				// Now we sync ANY new usage of the buffer by waiting until all transfer write commands in the command stream are completed on it.
				VkBufferMemoryBarrier2 postCopyBarrier = {
					.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,

					.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
					.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,

					.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
					.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT |
									 VK_ACCESS_2_MEMORY_WRITE_BIT,

					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

					.buffer = buffer,
					.offset = barrierSpanStart,
					.size = barrierSpanEnd - barrierSpanStart
				};

				cmdBuffer.Barrier({}, {}, { postCopyBarrier });
			}

			if (clear)
			{
				this->ClearAllocations();
			}
		}

		void ClearAllocations()
		{
			m_Allocations.clear();
		}
		
	private:
		void NextFrame()
		{
			this->ClearAllocations();
			m_Allocator.Rewind();
		}

		RenderAPI::Core* m_diCore = nullptr;
		RenderAPI::Buffer m_StagingBuffer;
		aZero::LinearAllocator<> m_Allocator;
		std::unordered_map<VkBuffer, AllocationInfo> m_Allocations;
	};
}