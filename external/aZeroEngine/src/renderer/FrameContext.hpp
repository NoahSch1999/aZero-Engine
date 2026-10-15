#pragma once
#include "render_api/Core.hpp"
#include "render_api/Fence.hpp"
#include "render_api/Semaphore.hpp"
#include "render_api/CommandQueue.hpp"

namespace aZero::Renderer
{
	class FrameContext
	{
	public:
		FrameContext() = default;
		FrameContext(RenderAPI::Core& core)
		{
			m_diCore = &core;
		}

		VkResult CheckCompleted(RenderAPI::Semaphore& semaphore, uint64_t timeout = UINT64_MAX)
		{
			VkSemaphore sem = semaphore.Get();
			VkSemaphoreWaitInfo wi = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
			wi.semaphoreCount = 1;
			wi.pSemaphores = &sem;
			wi.pValues = &m_FrameCompleteValue;

			const VkResult result = vkWaitSemaphores(m_diCore->GetLogicalDevice(), &wi, UINT64_MAX);
			if (result == VkResult::VK_TIMEOUT)
			{
				return result;
			}
			else if (result != VkResult::VK_SUCCESS && result != VkResult::VK_TIMEOUT)
			{
				RenderAPI::chkSucceed(result);
				return result;
			}
		}

		void SignalComplete(RenderAPI::CommandQueue& queue, RenderAPI::Semaphore& semaphore)
		{
			aZero::RenderAPI::CommandQueue::SubmitDesc submitDesc{
				.SignalSemaphores = {{ .SemaphorePtr = &semaphore, .Stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}}
			};
			queue.SubmitCommandBuffers(submitDesc);

			m_FrameCompleteValue = semaphore.GetLatestSignal_Timeline();
		}

	private:
		uint64_t m_FrameCompleteValue = 0;
		RenderAPI::Core* m_diCore = nullptr;
	};
}