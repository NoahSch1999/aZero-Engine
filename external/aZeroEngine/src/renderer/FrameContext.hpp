#pragma once
#include "render_api/Core.hpp"
#include "render_api/Fence.hpp"
#include "render_api/Semaphore.hpp"
#include "render_api/CommandQueue.hpp"
#include "FrameAllocator.hpp"

namespace aZero::Renderer
{
	class FrameContext
	{
	public:
		FrameContext() = default;
		FrameContext(RenderAPI::Core& core, aZero::Renderer::ResourceRecycler& recycler)
		{
			m_diCore = &core;
			m_Allocator = FrameAllocator(2048 * 2048, core, recycler);
		}

		VkResult CheckCompleted(RenderAPI::Semaphore& semaphore, uint64_t timeout = UINT64_MAX)
		{
			VkSemaphore sem = semaphore.Get();
			VkSemaphoreWaitInfo wi = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
			wi.semaphoreCount = 1;
			wi.pSemaphores = &sem;
			wi.pValues = &m_FrameCompleteValue;

			const VkResult result = vkWaitSemaphores(m_diCore->GetLogicalDevice(), &wi, timeout);
			if (result != VkResult::VK_SUCCESS && result != VkResult::VK_TIMEOUT)
			{
				RenderAPI::chkSucceed(result);
				return result;
			}

			if (result == VkResult::VK_SUCCESS)
			{
				m_Allocator.NextFrame();
			}
			return result;
		}

		void SignalComplete(RenderAPI::CommandQueue& queue, RenderAPI::Semaphore& semaphore)
		{
			aZero::RenderAPI::CommandQueue::SubmitDesc submitDesc{
				.SignalSemaphores = {{ .SemaphorePtr = &semaphore, .Stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}}
			};
			queue.SubmitCommandBuffers(submitDesc);

			m_FrameCompleteValue = semaphore.GetLatestSignal_Timeline();
		}

		FrameAllocator& GetFrameAllocator() { return m_Allocator; }

	private:
		uint64_t m_FrameCompleteValue = 0;
		RenderAPI::Core* m_diCore = nullptr;
		FrameAllocator m_Allocator;
	};
}