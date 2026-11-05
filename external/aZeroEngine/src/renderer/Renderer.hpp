#pragma once
#include "FrameContext.hpp"
#include "render_api/SwapChain.hpp"
#include "render_api/CommandQueue.hpp"
#include "CommandManager.hpp"
#include "asset/Material.hpp"
#include "asset/Texture.hpp"
#include "asset/Mesh.hpp"
#include "ResourceRecycler.hpp"
#include "render_api/Compiler.hpp"
#include "render_api/RenderPass.hpp"
#include "RenderTarget.hpp"

namespace aZero::Renderer
{
	class Renderer
	{
	public:
		Renderer() = default;
		~Renderer()
		{
			m_diCore->FlushGPU();
		}

		Compiler::ShaderProgram testShader;
		Pipeline::RenderPass testRenderPass;
		RenderAPI::Buffer testVB;

		Renderer(RenderAPI::Core& core, Compiler::Compiler& compiler, uint32_t frameBufferCount)
			:m_diCore(&core), m_diCompiler(&compiler)
		{
			m_FrameIndex = 0;

			aZero::Renderer::CommandManager::Desc cmdManagerDesc{
				.PoolDesc = {
					.QueueType = RenderAPI::EQueueType::Graphics
				},
				.FramesInFlight = frameBufferCount
			};
			m_CommandManager = aZero::Renderer::CommandManager(core, cmdManagerDesc, m_FrameIndex);

			for (uint32_t i = 0; i < frameBufferCount; i++)
			{
				m_FrameContexts.emplace_back(core);
			}

			m_GraphicsQueue.Init(core, RenderAPI::EQueueType::Graphics);
			m_RenderSemaphore.Init(core, RenderAPI::ESemaphoreType::Timeline);

			m_ResourceRecycler = aZero::Renderer::ResourceRecycler(*m_diCore, frameBufferCount, m_FrameIndex);

			Compiler::CompilerSession::Desc d;
			d.CompilerOptions = std::vector<slang::CompilerOptionEntry>({
				{
					slang::CompilerOptionName::EmitSpirvDirectly,
					{slang::CompilerOptionValueKind::Int, 1}
				},
				{
					slang::CompilerOptionName::DebugInformation,
					{slang::CompilerOptionValueKind::Int, SLANG_DEBUG_INFO_LEVEL_STANDARD}
				},
				{
					slang::CompilerOptionName::Optimization,
					{slang::CompilerOptionValueKind::Int, SLANG_OPTIMIZATION_LEVEL_NONE}
				}
				});

			Compiler::CompilerSession session = compiler.CreateSession(d).value();

			Compiler::ShaderModule testModule = session.LoadModule(std::string(ENGINE_SHADER_DIRECTORY) + "/test.slang").value();

			Compiler::ShaderProgram::VertexDesc testShaderDesc{
			   .EntryPoints = {"vsMain", "psMain"},
			   .Modules = {&testModule}
			};

			testShader.Build(testShaderDesc);

			Pipeline::RenderPass::VertexPassDesc vPassDesc{};
			vPassDesc.Program = &testShader;

			
			//vPassDesc.DepthDesc = Pipeline::RenderPass::Desc::DepthStencil();
			vPassDesc.RasterDesc = Pipeline::RenderPass::Desc::Rasterizer();
			vPassDesc.RasterDesc.CullMode = VkCullModeFlagBits::VK_CULL_MODE_NONE;
			vPassDesc.RenderTargetFormats = { VK_FORMAT_R8G8B8A8_SRGB };
			
			struct TempVertex
			{
				glm::vec3 Pos;
				glm::vec2 UV;
				TempVertex() = default;
				TempVertex(float px, float py, float pz, float uvx, float uvy)
				{
					Pos.x = px;
					Pos.y = py;
					Pos.z = pz;
					UV.x = uvx;
					UV.y = uvy;
				}
			};
			Pipeline::RenderPass::VertexPassDesc::VertexBinding bindings[2];
			bindings[0].AttributeFormats = { VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT };
			bindings[0].InputRate = VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX;
			bindings[0].Stride = sizeof(TempVertex);
			vPassDesc.VertexBindings = { bindings[0] };

			testRenderPass.Init(*m_diCore);
			testRenderPass.Compile(vPassDesc);
			
			RenderAPI::Buffer::Desc vbDesc{
				.Size = 1000,
				.UsageBits =
				VkBufferUsageFlagBits::VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
				VkBufferUsageFlagBits::VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
				VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT
				| VK_BUFFER_USAGE_TRANSFER_SRC_BIT
				,
				.MemoryType = RenderAPI::EMemoryType::VRAM,
				.SeperateHeap = true,
				.OptResourceRecycler = m_ResourceRecycler
			};

			testVB = RenderAPI::Buffer(core, vbDesc);

			RenderAPI::Buffer::Desc stagingBufferDesc{
				.Size = 1000,
				.UsageBits =
				VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				.MemoryType = RenderAPI::EMemoryType::SYSRAM,
				.SeperateHeap = false,
				.OptResourceRecycler = m_ResourceRecycler
			};
			RenderAPI::Buffer stagingBuffer(core, stagingBufferDesc);
			
			std::vector<TempVertex> vertices =
			{
				// Position                  // UV
				TempVertex (- 1.0f, -1.0f, 0.0f,0.0f, 1.0f),
				TempVertex(1.0f, -1.0f, 0.0f,      1.0f, 1.0f),
				TempVertex( 1.0f,  1.0f, 0.0f,      1.0f, 0.0f),
				TempVertex ( - 1.0f,  1.0f, 0.0f, 0.0f, 0.0f)
			};

			stagingBuffer.Upload(vertices, 0);

			std::vector<uint32_t> indices =
			{
				0, 1, 2,
				2, 3, 0
			};
			//
			stagingBuffer.Upload(indices, vertices.size() * sizeof(TempVertex));

			auto cb = m_CommandManager.GetBuffer();
			cb.StartRecording(VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
			cb.Copy(stagingBuffer, 0, testVB, 0, vertices.size() * sizeof(TempVertex) + indices.size() * sizeof(uint32_t));

			RenderAPI::Buffer::TransitionDesc tDesc{
				.DstStageMask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, //this can be specified more specificly
				.DstAccessMask = VkAccessFlagBits::VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VkAccessFlagBits::VK_ACCESS_INDEX_READ_BIT,
				.Offset = 0,
				.Size = (uint32_t)(vertices.size() * sizeof(TempVertex) + indices.size() * sizeof(uint32_t))
			};
			testVB.Transition(tDesc);
			
			cb.Barrier({}, {}, { testVB.GetTransition() });

			cb.StopRecording();

			aZero::Renderer::FrameContext& frameContext = m_FrameContexts[m_FrameIndex];
			aZero::RenderAPI::CommandQueue::SubmitDesc submitDesc{
			   .CommandBuffers = {&cb},
			   .OnCompletionFence = {},
			   .WaitOnSemaphores = {},
			   .SignalSemaphores = {{.SemaphorePtr = &m_RenderSemaphore, .Stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}}
			};
			m_GraphicsQueue.SubmitCommandBuffers(submitDesc);

			/*m_diCore->FlushGPU();

			cb.StartRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
			cb.Copy(testVB, 0, stagingBuffer, 0, vertices.size() * sizeof(TempVertex) + indices.size() * sizeof(uint32_t));
			cb.StopRecording();
			submitDesc = {
			   .CommandBuffers = {&cb},
			   .OnCompletionFence = {},
			   .WaitOnSemaphores = {},
			   .SignalSemaphores = {}
			};
			m_GraphicsQueue.SubmitCommandBuffers(submitDesc);
			m_diCore->FlushGPU();

			std::vector<TempVertex> idkVert;
			idkVert.resize(100);
			memcpy(idkVert.data(), stagingBuffer.GetMappedPtr(), vertices.size() * sizeof(TempVertex));
			std::vector<uint32_t> idkInd(100);
			memcpy(idkInd.data(), (std::byte*)stagingBuffer.GetMappedPtr() + vertices.size() * sizeof(TempVertex), indices.size() * sizeof(uint32_t));
			int dsdkldsk = 2;

			for (auto& idk : idkVert)
			{
				std::cout << idk.Pos.x << ", " << idk.Pos.y << ", " << idk.Pos.z << "\n";
			}*/
		}

		bool NextFrame(uint64_t waitTimeout = UINT64_MAX)
		{
			uint64_t lastFrameIndex = m_FrameIndex;

			if (!m_HasSignalledFrameComplete)
			{
				m_FrameContexts[lastFrameIndex].SignalComplete(m_GraphicsQueue, m_RenderSemaphore);
				m_HasSignalledFrameComplete = true;
			}

			uint64_t newFrameIndex = (m_FrameIndex + 1) % m_FrameContexts.size();

			aZero::Renderer::FrameContext& nextFrameContext = m_FrameContexts[newFrameIndex];
			const VkResult result = nextFrameContext.CheckCompleted(m_RenderSemaphore, waitTimeout);
			if (result != VkResult::VK_SUCCESS)
			{
				return false;
			}

			// This is updated AFTER the frame can start to be able to loop on the ::NextFrame() until its ready
			m_FrameIndex = newFrameIndex;
			m_HasSignalledFrameComplete = false; // We also reset this to false for the next ::NextFrame() call...

			m_CommandManager.NextFrame(m_FrameIndex);
			m_ResourceRecycler.NextFrame(m_FrameIndex);

			return true;
		}

		struct RenderDesc
		{
			aZero::Renderer::RenderTarget* ColorTarget;
			aZero::Renderer::RenderTarget* DepthStencil;
		};

		void Render(const RenderDesc& desc)
		{
			aZero::Renderer::FrameContext& frameContext = m_FrameContexts[m_FrameIndex];
			RenderAPI::CommandBuffer cb = m_CommandManager.GetBuffer();
			cb.StartRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

			if (
				desc.ColorTarget->m_Image.GetTransition().newLayout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
				)
			{
				RenderAPI::Image::TransitionDesc srcDesc {
				.DstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				.DstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.NewLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.SubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
				};


				desc.ColorTarget->m_Image.Transition(srcDesc);

				cb.Barrier({}, { desc.ColorTarget->m_Image.GetTransition() }, {});
			}

			Pipeline::RenderPass::RenderDesc::ColorTarget colorTarget{
				.View = desc.ColorTarget->m_View.Get()
			};
			Pipeline::RenderPass::RenderDesc renderDesc{};
			renderDesc.ColorTargets = { colorTarget };
			//renderDesc.DescriptorSets
			renderDesc.RenderHeight = desc.ColorTarget->m_Image.GetDesc().Height;
			renderDesc.RenderWidth = desc.ColorTarget->m_Image.GetDesc().Width;
			renderDesc.Callback = [&](RenderAPI::CommandBuffer& cbuff, aZero::Pipeline::RenderPass& pass) {
				auto b1 = pass.GetPushConstantBinding("Constants", "Color");
				cbuff.SetPushConstants(b1, glm::vec3(1, 0, 1));

				auto b2 = pass.GetPushConstantBinding("Constants", "VP");
				glm::mat4x4 vp; //
				cbuff.SetPushConstants(b2, vp);

				VkViewport viewport{
					   .x = 0.f,
					   .y = 0.f,
					   .width = static_cast<float>(desc.ColorTarget->m_Image.GetDesc().Width),
					   .height = static_cast<float>(desc.ColorTarget->m_Image.GetDesc().Height),
					   .minDepth = 0.0f,
					   .maxDepth = 1.0f
				};
				VkRect2D scissor{ .extent{.width = static_cast<uint32_t>(desc.ColorTarget->m_Image.GetDesc().Width), .height = static_cast<uint32_t>(desc.ColorTarget->m_Image.GetDesc().Height) } };
				cb.SetScizzorRectsAndViewports({ viewport }, { scissor });

				cb.SetVertexBuffer(testVB, 0);
				cb.SetIndexBuffer(testVB, 80);

				RenderAPI::CommandBuffer::DrawIndexedDesc drawdesc{
					.IndexCount = 6,
					.InstanceCount = 1,
					.FirstIndex = 0,
					.VertexOffset = 0,
					.FirstInstance = 0
				};
				cb.DrawIndexed(drawdesc);

			};
			testRenderPass.Render(cb, renderDesc);

			//VkClearColorValue color{};
			//color.float32[0] = 1.0f; // R
			//color.float32[1] = 0.0f; // G
			//color.float32[2] = 0.0f; // B
			//color.float32[3] = 1.0f; // A

			//VkImageSubresourceRange range{};
			//range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			//range.baseMipLevel = 0;
			//range.levelCount = 1;
			//range.baseArrayLayer = 0;
			//range.layerCount = 1;

			//desc.ColorTarget->Transition(
			//	{ 
			//	.DstStageMask = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			//	.DstAccessMask = VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT, 
			//	.NewLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
			//	.SubresourceRange = range 
			//	});

			//cb.Barrier({}, { desc.ColorTarget->GetTransition()}, {});

			//vkCmdClearColorImage(
			//	cb.Get(),
			//	desc.ColorTarget->Get(),
			//	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			//	&color,
			//	1,
			//	&range
			//);

			cb.StopRecording();

			aZero::RenderAPI::CommandQueue::SubmitDesc submitDesc{
			   .CommandBuffers = {&cb },
			   .WaitOnSemaphores = {{.SemaphorePtr = &m_RenderSemaphore, .Stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}},
			   .SignalSemaphores = {{.SemaphorePtr = &m_RenderSemaphore, .Stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}}
			};
			m_GraphicsQueue.SubmitCommandBuffers(submitDesc);
		}

		void CopyToSwapchain(aZero::Renderer::RenderTarget& srcTarget, aZero::RenderAPI::Swapchain& swapchain)
		{
			aZero::Renderer::FrameContext& frameContext = m_FrameContexts[m_FrameIndex];
			RenderAPI::CommandBuffer cb = m_CommandManager.GetBuffer();
			cb.StartRecording(VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

			VkImageMemoryBarrier2 scBarrier {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
				.srcAccessMask = VK_ACCESS_2_NONE,
				.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
				.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.image = swapchain.GetCurrentBackBuffer(),
				.subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
			};

			RenderAPI::Image::TransitionDesc srcDesc{
				.DstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
				.DstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
				.NewLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				.SubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
			};

			srcTarget.m_Image.Transition(srcDesc);

			cb.Barrier({}, { scBarrier, srcTarget.m_Image.GetTransition()}, {});

			cb.Copy(srcTarget.m_Image, swapchain.GetCurrentBackBuffer(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

			scBarrier = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
				.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_2_NONE,
				.dstAccessMask = VK_ACCESS_2_NONE,
				.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				.image = swapchain.GetCurrentBackBuffer(),
				.subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
			};

			srcDesc = {
				.DstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				.DstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.NewLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.SubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
			};

			srcTarget.m_Image.Transition(srcDesc);

			cb.Barrier({}, { scBarrier, srcTarget.m_Image.GetTransition() }, {});

			cb.StopRecording();

			aZero::RenderAPI::CommandQueue::SubmitDesc submitDesc{
			   .CommandBuffers = {&cb },
			   //.OnCompletionFence = frameContext.GetFence(),
			   .WaitOnSemaphores = {{.SemaphorePtr = swapchain.GetImageReadySemaphore(), .Stage = VK_PIPELINE_STAGE_2_COPY_BIT}, {.SemaphorePtr = &m_RenderSemaphore, .Stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}},
			   .SignalSemaphores = {{.SemaphorePtr = swapchain.GetRenderCompletedSemaphore(), .Stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}, {.SemaphorePtr = &m_RenderSemaphore, .Stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}}
			};
			m_GraphicsQueue.SubmitCommandBuffers(submitDesc);
		}

		std::optional<RenderTarget> CreateRenderTarget(const glm::uvec2& dimensions, ERenderTargetType type)
		{
			aZero::Renderer::RenderTarget rTarget;
			if (!rTarget.Init(type, dimensions, *m_diCore, m_ResourceRecycler))
			{
				return std::nullopt;
			}
			return rTarget;
		}

		uint32_t GetFrameIndex() const { return m_FrameIndex; }
		FrameContext& GetCurrentContext() { return m_FrameContexts.at(m_FrameIndex); }
		uint32_t GetFramesInFlight() const { return m_FrameContexts.size(); }

		// This is specialized to define custom register-behavior on a per-asset level
		template<typename AssetType>
		void RegisterOrUpdateAsset(AssetType& asset);

		// This is specialized to define custom unregister-behavior on a per-asset level
		template<typename AssetType>
		void UnregisterAsset(AssetType& asset);

	private:
		bool m_HasSignalledFrameComplete = false;
		RenderAPI::Core* m_diCore = nullptr;
		Compiler::Compiler* m_diCompiler = nullptr;
		aZero::Renderer::CommandManager m_CommandManager;
		std::vector<aZero::Renderer::FrameContext> m_FrameContexts;
		RenderAPI::CommandQueue m_GraphicsQueue;
		uint32_t m_FrameIndex;
		RenderAPI::Semaphore m_RenderSemaphore;
		aZero::Renderer::ResourceRecycler m_ResourceRecycler;
	};
}

template<typename AssetType>
inline void aZero::Renderer::Renderer::RegisterOrUpdateAsset(AssetType&) {}

template<typename AssetType>
inline void aZero::Renderer::Renderer::UnregisterAsset(AssetType&) {}

template<>
inline void aZero::Renderer::Renderer::RegisterOrUpdateAsset<aZero::Asset::Mesh>(aZero::Asset::Mesh& mesh)
{
	//if (mesh.GetRenderRef())
	//{
	//	// todo Impl update of existing asset
	//	return;
	//}
	FrameContext& context = this->GetCurrentContext();
	/*const auto [meshletOffset, vertexOffset] = m_RenderAssetManager->UpdateRenderState(context.GetFrameStagingAllocator(),
		mesh.GetCachedData().m_VertexData.Meshlets, mesh.GetCachedData().m_VertexData.Vertices, mesh.GetCachedData().m_VertexData.MeshletBounds);
	mesh.m_RenderRef.m_MeshletGlobalOffset = meshletOffset;
	mesh.m_RenderRef.m_VertexGlobalOffset = vertexOffset;*/
}

template<>
inline void aZero::Renderer::Renderer::UnregisterAsset<aZero::Asset::Mesh>(aZero::Asset::Mesh& mesh)
{
	//m_RenderAssetManager->RemoveMeshAsset(mesh.GetRenderRef().m_MeshletGlobalOffset);
}

template<>
inline void aZero::Renderer::Renderer::RegisterOrUpdateAsset<aZero::Asset::Material>(aZero::Asset::Material& material)
{
	FrameContext& context = this->GetCurrentContext();

	if (material.GetAlbedoPtr() && !material.GetAlbedoPtr()->GetRenderRef().IsValid())
	{
		// todo Impl handling of textures non-valid textures
		throw std::invalid_argument("No bound valid albedo texture");
	}

	// todo Maybe call different overloads based on material properties?
	//material.m_RenderRef.MaterialIndex = m_RenderAssetManager->UpdateRenderState(context.GetFrameStagingAllocator(), material.m_RenderRef.MaterialIndex, material.GetFormat_PBR_GPU());
}

template<>
inline void aZero::Renderer::Renderer::UnregisterAsset<aZero::Asset::Material>(aZero::Asset::Material& material)
{
	//m_RenderAssetManager->RemoveMaterialAsset(material.GetRenderRef().MaterialIndex);
}

template<>
inline void aZero::Renderer::Renderer::RegisterOrUpdateAsset<aZero::Asset::Texture>(Asset::Texture& texture)
{
	if (texture.GetRenderRef().IsValid())
	{
		// todo Impl update of existing asset
		return;
	}
	FrameContext& context = this->GetCurrentContext();
	/*texture.m_RenderRef.DescriptorIndex = m_RenderAssetManager->UpdateRenderState(m_diDevice, context.GetCommandList(), m_ResourceRecycler, m_ResourceHeap, texture.GetCachedData());
	m_DirectCommandQueue.ExecuteCommandList(context.GetCommandList());*/
}

template<>
inline void aZero::Renderer::Renderer::UnregisterAsset<aZero::Asset::Texture>(aZero::Asset::Texture& texture)
{
	//m_RenderAssetManager->RemoveTextureAsset(texture.GetRenderRef().DescriptorIndex);

	// todo Impl recycle that doesnt force flush of descriptors
	this->m_diCore->FlushGPU();
}