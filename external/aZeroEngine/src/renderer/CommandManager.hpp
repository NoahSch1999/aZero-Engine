#pragma once
#include "render_api/Core.hpp"
#include "render_api/Fence.hpp"
#include "render_api/CommandPool.hpp"

namespace aZero::Renderer
{
	class CommandManager
	{
	public:
		struct Desc
		{
			RenderAPI::CommandPool::Desc PoolDesc;
			uint32_t FramesInFlight;
		};

		CommandManager() = default;
		CommandManager(RenderAPI::Core& core, const Desc& desc, uint32_t startIndex)
		{
			m_CommandPool = RenderAPI::CommandPool(core, desc.PoolDesc);
			m_FrameData.resize(desc.FramesInFlight);
			m_FrameIndex = startIndex;
		}

		CommandManager(CommandManager&& other) noexcept
		{
			*this = std::move(other);
		}

		CommandManager& operator=(CommandManager&& other) noexcept
		{
			std::swap(m_FrameData, other.m_FrameData);
			std::swap(m_CommandPool, other.m_CommandPool);
			std::swap(m_FrameIndex, other.m_FrameIndex);
			return *this;
		}

		/*
		If we have unused buffers we return an existing one, else create a new and return it.
		*/
		RenderAPI::CommandBuffer GetBuffer()
		{
			if (m_FrameData[m_FrameIndex].InUse == m_FrameData[m_FrameIndex].Buffers.size())
			{
				auto newBuffers = m_CommandPool.CreateCommandBuffers(1);
				m_FrameData[m_FrameIndex].Buffers.insert(m_FrameData[m_FrameIndex].Buffers.end(), newBuffers.begin(), newBuffers.end());
				m_FrameData[m_FrameIndex].InUse++;
				return newBuffers[0];
			}

			auto buffer = m_FrameData[m_FrameIndex].Buffers[m_FrameData[m_FrameIndex].InUse];
			m_FrameData[m_FrameIndex].InUse++;
			return buffer;
		}

		/*
		1. Resets buffers for this frame (::NextFrame should be called when we know that these commandbuffers have finished being executed)
		2. Reset internal counter for frame to 0 so we know where to get the next requested buffer
		*/
		void NextFrame(uint32_t frameIndex, bool clearMemory = false)
		{
			m_FrameIndex = frameIndex;
			for (auto& buffer : m_FrameData[m_FrameIndex].Buffers)
			{
				buffer.Reset();
			}
			m_FrameData[m_FrameIndex].InUse = 0;

			if (clearMemory)
			{
				m_FrameData[m_FrameIndex].Buffers.clear();
			}
		}

	private:
		struct FrameData
		{
			std::vector<RenderAPI::CommandBuffer> Buffers;
			uint32_t InUse = 0;
		};
		std::vector<FrameData> m_FrameData;
		RenderAPI::CommandPool m_CommandPool;
		uint32_t m_FrameIndex = 0;
	};
}