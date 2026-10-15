#pragma once
#include "Fence.hpp"
#include "Semaphore.hpp"
#include "CommandPool.hpp"
#include "Buffer.hpp"

namespace aZero::RenderAPI
{
    class CommandQueue
    {
    public:
        CommandQueue() = default;
        CommandQueue(Core& core, EQueueType type)
        {
            this->Init(core, type);
        }

        void Init(Core& core, EQueueType type)
        {
            vkGetDeviceQueue(core.GetLogicalDevice(), core.GetIndexForQueueType(type), 0, &m_Queue);
            m_diCore = &core;
        }

        VkQueue Get() const { return m_Queue; }

        struct SubmitDesc
        {
            struct SubmitSemaphore
            {
                Semaphore* SemaphorePtr;
                VkPipelineStageFlags2 Stage;
            };

            std::vector<RenderAPI::CommandBuffer*> CommandBuffers;

            /*
            when the entire submission has completed we make the fence be "ready"/"completed".
            used for cpu-side waits.
            */
            std::optional<std::reference_wrapper<Fence>> OnCompletionFence;

            /*
            for each:
                if non-timeline:
                    dont start the given stage in this submission until this semaphore has been signalled.
                    when this stage is starting the semaphore will be unsignalled.
                if timeline:
                    dont start the given stage in this submission until this semaphore has reached "Value", aka current >= value.
            */
            std::vector<SubmitSemaphore> WaitOnSemaphores;

            /*
            for each:
                if non-timeline:
                    signal the semaphore when the given stage in this submission (or earlier submissions in the same queue) has been completed.
                if timeline:
                    set the timeline semaphore to "Value" when the given stage in this submission (or earlier submissions in the same queue) has been completed.
            */
            std::vector<SubmitSemaphore> SignalSemaphores;

            void* pNext = nullptr;
        };



        bool SubmitCommandBuffers(SubmitDesc& submitDesc)
        {
            std::vector<VkSemaphoreSubmitInfo> waitSemaphores, signalSemaphores;

            for (SubmitDesc::SubmitSemaphore& semaphore : submitDesc.WaitOnSemaphores)
            {
                VkSemaphoreSubmitInfo info{
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = semaphore.SemaphorePtr->Get(),
                    .stageMask = semaphore.Stage
                };

                if (semaphore.SemaphorePtr->GetType() == RenderAPI::ESemaphoreType::Timeline) {
                    info.value = semaphore.SemaphorePtr->GetLatestSignal_Timeline();
                }

                waitSemaphores.push_back(info);
            }

            for (SubmitDesc::SubmitSemaphore& semaphore : submitDesc.SignalSemaphores)
            {
                VkSemaphoreSubmitInfo info{
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .semaphore = semaphore.SemaphorePtr->Get(),
                    .stageMask = semaphore.Stage
                };

                if (semaphore.SemaphorePtr->GetType() == RenderAPI::ESemaphoreType::Timeline) {
                    info.value = semaphore.SemaphorePtr->Signal_Timeline(); // This is the new sync-point on the timeline
                }

                signalSemaphores.push_back(info);
            }

            std::vector<VkCommandBufferSubmitInfo> cbInfos;
            for (RenderAPI::CommandBuffer* buffer : submitDesc.CommandBuffers)
            {
                VkCommandBufferSubmitInfo info{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                    .commandBuffer = buffer->Get()
                };
                cbInfos.push_back(info);
            }

            VkSubmitInfo2 submitInfo{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .pNext = submitDesc.pNext,
                .waitSemaphoreInfoCount = (uint32_t)waitSemaphores.size(),
                .pWaitSemaphoreInfos = waitSemaphores.data(),
                .commandBufferInfoCount = (uint32_t)cbInfos.size(),
                .pCommandBufferInfos = cbInfos.data(),
                .signalSemaphoreInfoCount = (uint32_t)signalSemaphores.size(),
                .pSignalSemaphoreInfos = signalSemaphores.data()
            };

            return chkSucceed(vkQueueSubmit2(m_Queue, 1, &submitInfo, submitDesc.OnCompletionFence.has_value() ? submitDesc.OnCompletionFence.value().get().Get() : nullptr));
        }

    private:
        RenderAPI::Core* m_diCore = nullptr;
        VkQueue m_Queue; 
        EQueueType m_Type;
    };
}