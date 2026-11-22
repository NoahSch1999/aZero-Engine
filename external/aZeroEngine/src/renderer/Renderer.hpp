#pragma once
#include "FrameContext.hpp"
#include "render_api/SwapChain.hpp"
#include "render_api/CommandQueue.hpp"
#include "CommandManager.hpp"
#include "ResourceRecycler.hpp"
#include "render_api/Compiler.hpp"
#include "render_api/RenderPass.hpp"
#include "RenderTarget.hpp"
#include "system/MeshSystem.hpp"
#include "system/MaterialSystem.hpp"
#include "system/TextureSystem.hpp"
#include "scene/components/CameraComponent.hpp"
#include "scene/Scene.hpp"
#include "mesh_draw_pipeline/Mesh_Draw_Structs.hpp"
#include "asset/loading_saving/gltf.hpp"

namespace aZero { class Engine; }
namespace aZero::Renderer
{
	enum class ETonemapType { NONE = 0, REINHARD = 1, ACES = 2 };

	class Renderer
	{
		friend class aZero::Engine;
	public:

		VkDeviceAddress testMaterialPtr; // temp

		Renderer() = default;
		~Renderer()
		{
			this->FlushRenderingCommands();
		}

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

			m_ResourceRecycler = std::make_unique<aZero::Renderer::ResourceRecycler>(*m_diCore, frameBufferCount, m_FrameIndex);
			for (uint32_t i = 0; i < frameBufferCount; i++)
			{
				m_FrameContexts.emplace_back(core, *m_ResourceRecycler);
			}

			m_GraphicsQueue.Init(core, RenderAPI::EQueueType::Graphics);
			m_RenderSemaphore.Init(core, RenderAPI::ESemaphoreType::Timeline);

			m_MeshSystem = std::make_unique<System::MeshSystem>(*m_diCore, *m_ResourceRecycler);
			m_TextureSystem = std::make_unique<System::TextureSystem>(*m_diCore, *m_ResourceRecycler, frameBufferCount);
			m_MaterialSystem = std::make_unique<System::MaterialSystem>(*m_diCore, compiler, *m_ResourceRecycler, (uint64_t)(2048 * 2048), 1000, *m_TextureSystem.get());


			{
				RenderAPI::Buffer::Desc desc{
					.Size = 10000 * sizeof(GPUStruct::MeshTaskIndirectArgs),
					.UsageBits =
						VkBufferUsageFlagBits::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
						VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
						VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
					.MemoryType = RenderAPI::EMemoryType::VRAM,
					.SeperateHeap = true,
					.Name = "m_MeshDraw_IA_Buffer",
					.OptResourceRecycler = *m_ResourceRecycler.get()
				};

				m_MeshDraw_IA_Buffer = RenderAPI::Buffer(core, desc);
			}
			{
				RenderAPI::Buffer::Desc desc{
					.Size = 10000 * sizeof(GPUStruct::IndirectArgumentCounter),
					.UsageBits =
						VkBufferUsageFlagBits::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
						VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
						VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT |
						VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
					.MemoryType = RenderAPI::EMemoryType::VRAM,
					.SeperateHeap = true,
					.Name = "m_MeshDraw_IA_Counter_Buffer",
					.OptResourceRecycler = *m_ResourceRecycler.get()
				};

				m_MeshDraw_IA_Counter_Buffer = RenderAPI::Buffer(core, desc);
			}
			{
				RenderAPI::Buffer::Desc desc{
					.Size = 10000 * sizeof(GPUStruct::DrawMetaData),
					.UsageBits =
						VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
					.MemoryType = RenderAPI::EMemoryType::VRAM,
					.SeperateHeap = true,
					.Name = "m_MeshDraw_DrawMetadata_Buffer",
					.OptResourceRecycler = *m_ResourceRecycler.get()
				};

				m_MeshDraw_DrawMetadata_Buffer = RenderAPI::Buffer(core, desc);
			}

			this->NextFrame();
			this->InitAssetSystemDefaults();
			this->InitRenderTargetDescriptorPool();
			this->TestInit();
			this->RebuildComputePipeline();
			this->InitFullscreenQuad();
			this->FinalizeFrame();
			this->FlushRenderingCommands(); // Ensures that all init stuff is ready for the cpu to consume
		}

		void RebuildComputePipeline()
		{
			Compiler::CompilerSession::Desc sessionDesc{
				.IncludePaths = std::vector<std::filesystem::path>(
				{
						std::filesystem::path(std::string(ENGINE_SHADER_DIRECTORY) + "/core_shaders/")
				}
				),
				.CompilerOptions = std::vector<slang::CompilerOptionEntry>({
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
					{slang::CompilerOptionValueKind::Int, SLANG_OPTIMIZATION_LEVEL_NONE} // todo Change based on build
				}
				})
			};

			m_Session = m_diCompiler->CreateSession(sessionDesc).value();

			auto res = m_Session.LoadModule(std::string(ENGINE_SHADER_DIRECTORY) + "/core_shaders/mesh_draw_pipeline/ComputeCull.slang");
			if (res)
			{
				Compiler::ShaderProgram::EntryPointDesc eDesc{ .Name = "main" };
				Compiler::ShaderProgram::ComputeDesc csProgramDesc{
					.EntryPoint = eDesc,
					.Modules = { &res.value()}
				};

				Compiler::ShaderProgram program;
				program.Build(csProgramDesc);

				Pipeline::RenderPass pass(*m_diCore);
				Pipeline::RenderPass::ComputePassDesc csCullPassDesc{
					.Program = &program
				};
				if (pass.Compile(csCullPassDesc))
				{
					m_ComputeCullPass = std::move(pass);
				}
			}
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
			m_ResourceRecycler->NextFrame(m_FrameIndex);

			m_FrameDataUploader = m_CommandManager.GetBuffer();
			m_FrameDataUploader.StartRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

			m_MeshSystem->NextFrame(m_FrameDataUploader, m_FrameContexts[m_FrameIndex].GetFrameAllocator());
			m_TextureSystem->NextFrame(m_FrameIndex, m_FrameDataUploader);

			return true;
		}

		void FinalizeFrame()
		{
			aZero::Renderer::FrameContext& frameContext = m_FrameContexts[m_FrameIndex];

			m_TextureSystem->UpdateDirtyTextures();
			m_MeshSystem->UpdateDirtyMeshes();
			m_MaterialSystem->UpdateDirtyMaterials(frameContext.GetFrameAllocator());

			frameContext.GetFrameAllocator().Record(m_FrameDataUploader);
			m_FrameDataUploader.StopRecording();

			aZero::RenderAPI::CommandQueue::SubmitDesc submitDesc{
			   .CommandBuffers = {&m_FrameDataUploader},
			   .WaitOnSemaphores = {}, // No need to wait on the m_RenderSemaphore since barriers will guarantee sync if we use them correctly...
			   .SignalSemaphores = {{.SemaphorePtr = &m_RenderSemaphore, .Stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}}
			};
			
			m_GraphicsQueue.SubmitCommandBuffers(submitDesc);
		}

		bool PresentSwapchain(aZero::Renderer::RenderTarget& srcTarget, aZero::RenderAPI::Swapchain& swapchain, float exposure = 1.f, ETonemapType tonemap = ETonemapType::REINHARD, bool copyRawFormat = false /* If true -> vkCmdBlitImage. If false -> HDR + tonemap etc */)
		{
			if (!swapchain.AcquireNextImage())
			{
				// If we cant acquire the next image we dont copy or render to the swapchain
				return false;
			}

			if (copyRawFormat)
			{
				aZero::Renderer::FrameContext& frameContext = m_FrameContexts[m_FrameIndex];
				RenderAPI::CommandBuffer cb = m_CommandManager.GetBuffer();
				cb.StartRecording(VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

				RenderAPI::Image::TransitionDesc scDesc{
					.DstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
					.DstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
					.NewLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.SubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
				};

				swapchain.GetCurrentBackBuffer().Transition(scDesc);

				RenderAPI::Image::TransitionDesc srcDesc{
					.DstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
					.DstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
					.NewLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					.SubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
				};

				srcTarget.m_Image.Transition(srcDesc);

				cb.Barrier({}, { swapchain.GetCurrentBackBuffer().GetTransition(), srcTarget.m_Image.GetTransition()}, {});

				std::vector<VkImageBlit> blits(1);
				blits[0] = {
					.srcSubresource = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.mipLevel = 0,
						.baseArrayLayer = 0,
						.layerCount = 1
					},
					.srcOffsets = {
						{.x = 0, .y = 0, .z = 0},
						{.x = (int32_t)srcTarget.GetDimensions().x, .y = (int32_t)srcTarget.GetDimensions().y, .z = 1}
					},
					.dstSubresource = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.mipLevel = 0,
						.baseArrayLayer = 0,
						.layerCount = 1
					},
					.dstOffsets = {
						{.x = 0, .y = 0, .z = 0},
						{.x = (int32_t)swapchain.m_CreateInfo.imageExtent.width, .y = (int32_t)swapchain.m_CreateInfo.imageExtent.height, .z = 1}
					},
				};
				cb.BlitImages(srcTarget.m_Image, swapchain.GetCurrentBackBuffer(), blits, VkFilter::VK_FILTER_LINEAR);
				
				scDesc = {
					.DstStageMask = VK_PIPELINE_STAGE_2_NONE,
					.DstAccessMask = VK_ACCESS_2_NONE,
					.NewLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
					.SubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
				};

				swapchain.GetCurrentBackBuffer().Transition(scDesc);

				srcDesc = {
					.DstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
					.DstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
					.NewLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.SubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
				};

				srcTarget.m_Image.Transition(srcDesc);

				cb.Barrier({}, { swapchain.GetCurrentBackBuffer().GetTransition(), srcTarget.m_Image.GetTransition() }, {});

				cb.StopRecording();

				aZero::RenderAPI::CommandQueue::SubmitDesc submitDesc{
					.CommandBuffers = {&cb },
					// Wait at "acquire image semaphore" when a blit-stage is first reached.
					// Barriers handle accesses to the render target, so no need to wait on a semaphore for that.
					.WaitOnSemaphores = {{.SemaphorePtr = swapchain.GetImageReadySemaphore(), .Stage = VK_PIPELINE_STAGE_2_BLIT_BIT}},

					// Signal commands complete when all commands are done.
					.SignalSemaphores = {{.SemaphorePtr = swapchain.GetRenderCompletedSemaphore(), .Stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}, {.SemaphorePtr = &m_RenderSemaphore, .Stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}}
				};
				m_GraphicsQueue.SubmitCommandBuffers(submitDesc);

				return swapchain.Present(m_GraphicsQueue);
			}
			else
			{
				this->RecordFullscreenPass(srcTarget, swapchain, exposure, tonemap);

				return swapchain.Present(m_GraphicsQueue);
			}

			return true;
		}

		std::optional<RenderTarget> CreateRenderTarget(const glm::uvec2& dimensions, ERenderTargetType type)
		{
			aZero::Renderer::RenderTarget rTarget;
			if (!rTarget.Init(type, dimensions, *m_diCore, *m_ResourceRecycler, m_SampledRenderTargetFreelist, m_RenderTargetDescriptorSet))
			{
				return std::nullopt;
			}

			if (type == ERenderTargetType::DEPTH_STENCIL)
			{
				aZero::RenderAPI::Image::TransitionDesc trDesc{
					.DstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
					.DstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
					.NewLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				};
				trDesc.SubresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
				rTarget.m_Image.Transition(trDesc);
				m_FrameDataUploader.Barrier({}, { rTarget.m_Image.GetTransition() }, {});

				// todo Create view for it
			}
			else if(type == ERenderTargetType::COLOR)
			{
				aZero::RenderAPI::Image::TransitionDesc trDesc{
					.DstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
					.DstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
					.NewLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				};
				trDesc.SubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				rTarget.m_Image.Transition(trDesc);
				m_FrameDataUploader.Barrier({}, { rTarget.m_Image.GetTransition() }, {});

				// todo Create view for it
			}
			return rTarget;
		}

		void RenderScene(Scene::Scene& scene)
		{
			const std::vector<uint32_t> perMaterialCounts = m_MaterialSystem->GetMaterialTypeCounts();
			std::vector<uint32_t> materialIndirectArgIndexOffsets(perMaterialCounts.size());
			std::exclusive_scan(perMaterialCounts.cbegin(), perMaterialCounts.cend(), materialIndirectArgIndexOffsets.begin(), 0); // Prefix-sum to get the start offset of each materialtype IA

			aZero::Renderer::FrameContext& frameContext = m_FrameContexts[m_FrameIndex];
			RenderAPI::CommandBuffer commandBuffer = m_CommandManager.GetBuffer();
			commandBuffer.StartRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

			auto [frameData, renderData] = scene.CreateRenderData(*m_diCore, *m_ResourceRecycler.get(), frameContext.GetFrameAllocator().GetAllocator(), frameContext.GetFrameAllocator().GetBuffer(), commandBuffer);
			for (uint32_t i = 0; i < frameData.Cameras.size(); i++) // todo For each camera
			{
				const Component::CameraComponent& camera = *frameData.Cameras[i];
				if (camera.IsValidForRendering())
				{
					this->RenderWithCamera(commandBuffer, materialIndirectArgIndexOffsets, perMaterialCounts, renderData, frameData, i);
				}
			}

			commandBuffer.StopRecording();

			aZero::RenderAPI::CommandQueue::SubmitDesc submitDesc{
			   .CommandBuffers = { &commandBuffer },
			   .WaitOnSemaphores = {}, // No need to wait on the m_RenderSemaphore since barriers will guarantee sync if we use them correctly...
			   .SignalSemaphores = {{.SemaphorePtr = &m_RenderSemaphore, .Stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}}
			};
			m_GraphicsQueue.SubmitCommandBuffers(submitDesc);
		}

		void ClearDepthStencilTarget(RenderTarget& renderTarget, const VkClearDepthStencilValue& value)
		{
			RenderAPI::CommandBuffer commandBuffer = m_CommandManager.GetBuffer();
			commandBuffer.StartRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

			{
				aZero::RenderAPI::Image::TransitionDesc trDesc{
						.DstStageMask = VK_PIPELINE_STAGE_2_CLEAR_BIT,
						.DstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
						.NewLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				};
				trDesc.SubresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
				renderTarget.m_Image.Transition(trDesc);
			}
			commandBuffer.Barrier({}, { renderTarget.m_Image.GetTransition() }, {});

			commandBuffer.ClearDepthStencilTarget(renderTarget.m_Image, value);

			{
				aZero::RenderAPI::Image::TransitionDesc trDesc{
							.DstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
							.DstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
							.NewLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				};
				trDesc.SubresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
				renderTarget.m_Image.Transition(trDesc);
			}
			commandBuffer.Barrier({}, { renderTarget.m_Image.GetTransition() }, {});

			commandBuffer.StopRecording();

			aZero::RenderAPI::CommandQueue::SubmitDesc submitDesc{
			   .CommandBuffers = { &commandBuffer },
			   .WaitOnSemaphores = {}, // No need to wait on the m_RenderSemaphore since barriers will guarantee sync if we use them correctly...
			   .SignalSemaphores = {{.SemaphorePtr = &m_RenderSemaphore, .Stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}}
			};
			m_GraphicsQueue.SubmitCommandBuffers(submitDesc);
		}

		void ClearColorTarget(RenderTarget& renderTarget, const VkClearColorValue& color)
		{
			RenderAPI::CommandBuffer commandBuffer = m_CommandManager.GetBuffer();
			commandBuffer.StartRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

			{
				aZero::RenderAPI::Image::TransitionDesc trDesc{
							.DstStageMask = VK_PIPELINE_STAGE_2_CLEAR_BIT,
							.DstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
							.NewLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				};
				trDesc.SubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				renderTarget.m_Image.Transition(trDesc);
			}
			commandBuffer.Barrier({}, { renderTarget.m_Image.GetTransition() }, {});

			commandBuffer.ClearColorTarget(renderTarget.m_Image, color);

			{
				aZero::RenderAPI::Image::TransitionDesc trDesc{
							.DstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
							.DstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
							.NewLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				};
				trDesc.SubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				renderTarget.m_Image.Transition(trDesc);
			}
			commandBuffer.Barrier({}, { renderTarget.m_Image.GetTransition() }, {});

			commandBuffer.StopRecording();

			aZero::RenderAPI::CommandQueue::SubmitDesc submitDesc{
			   .CommandBuffers = { &commandBuffer },
			   .WaitOnSemaphores = {}, // No need to wait on the m_RenderSemaphore since barriers will guarantee sync if we use them correctly...
			   .SignalSemaphores = {{.SemaphorePtr = &m_RenderSemaphore, .Stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}}
			};
			m_GraphicsQueue.SubmitCommandBuffers(submitDesc);
		}

		void SubmitCommandBuffer(RenderAPI::CommandBuffer& commandBuffer)
		{
			aZero::RenderAPI::CommandQueue::SubmitDesc submitDesc{
			   .CommandBuffers = { &commandBuffer },
			   .WaitOnSemaphores = {},
			   .SignalSemaphores = {{.SemaphorePtr = &m_RenderSemaphore, .Stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}}
			};
			m_GraphicsQueue.SubmitCommandBuffers(submitDesc);
		}

		RenderAPI::CommandQueue& GetGraphicsQueue() { return m_GraphicsQueue; }
		uint32_t GetFrameIndex() const { return m_FrameIndex; }
		FrameContext& GetCurrentContext() { return m_FrameContexts.at(m_FrameIndex); }
		uint32_t GetFramesInFlight() const { return m_FrameContexts.size(); }
		void FlushRenderingCommands() const { m_diCore->FlushGPU(); }
		aZero::Renderer::CommandManager& GetCommandManager() { return m_CommandManager; }

	private:
		void RecordMeshCullPass(RenderAPI::CommandBuffer& commandBuffer, const std::vector<uint32_t>& materialOffsets, const Scene::SceneRenderData& renderData, const Scene::SceneRenderDataFrameInfo& frameData, uint32_t cameraIndex)
		{
			aZero::Renderer::FrameContext& frameContext = m_FrameContexts[m_FrameIndex];

			{
				aZero::RenderAPI::Buffer::TransitionDesc trDesc{
					.DstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
					.DstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
					.Offset = 0,
					.Size = m_MeshDraw_IA_Counter_Buffer.GetDesc().Size
				};
				m_MeshDraw_IA_Counter_Buffer.Transition(trDesc);
			}

			commandBuffer.Barrier({}, {}, { m_MeshDraw_IA_Counter_Buffer.GetTransition() });
			frameContext.GetFrameAllocator().RecordUpload(commandBuffer, (void*)materialOffsets.data(), materialOffsets.size() * sizeof(materialOffsets[0]), m_MeshDraw_IA_Counter_Buffer, 0);

			{
				aZero::RenderAPI::Buffer::TransitionDesc trDesc{
					.DstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					.DstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
					.Offset = 0,
					.Size = m_MeshDraw_IA_Counter_Buffer.GetDesc().Size
				};
				m_MeshDraw_IA_Counter_Buffer.Transition(trDesc);
			}

			{
				aZero::RenderAPI::Buffer::TransitionDesc trDesc{
					.DstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					.DstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
					.Offset = 0,
					.Size = m_MeshDraw_IA_Buffer.GetDesc().Size
				};
				m_MeshDraw_IA_Buffer.Transition(trDesc);
			}

			{
				aZero::RenderAPI::Buffer::TransitionDesc trDesc{
					.DstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					.DstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
					.Offset = 0,
					.Size = m_MeshDraw_DrawMetadata_Buffer.GetDesc().Size
				};
				m_MeshDraw_DrawMetadata_Buffer.Transition(trDesc);
			}

			commandBuffer.Barrier({}, {}, { m_MeshDraw_IA_Counter_Buffer.GetTransition(), m_MeshDraw_IA_Buffer.GetTransition(), m_MeshDraw_DrawMetadata_Buffer.GetTransition()});

			// todo GroupX should be set so theres enough threads for all entities
			m_ComputeCullPass.Render(commandBuffer, {
		   .GroupX = (uint32_t)std::ceil(frameData.MeshCount / 32.f), .GroupY = 1, .GroupZ = 1,
		   .DescriptorSets = {},
		   .Callback = [&](RenderAPI::CommandBuffer& cb, Pipeline::RenderPass& pass) {
					GPUStruct::ComputeCullConstants constants{
						.IndirectArgumentBufferPtr = m_MeshDraw_IA_Buffer.GetDeviceAddress().value(),
						.IndirectArgumentCounterBufferPtr = m_MeshDraw_IA_Counter_Buffer.GetDeviceAddress().value(),
						.CameraBufferPtr = renderData.CameraBuffer.GetDeviceAddress().value() + cameraIndex * sizeof(GPUStruct::CameraData),
						.InstanceDataBufferPtr = renderData.InstanceBuffer.GetDeviceAddress().value(),
						.InstanceCullDataBufferPtr = renderData.InstanceCullDataBuffer.GetDeviceAddress().value(),
						.DrawMetadataBufferPtr = m_MeshDraw_DrawMetadata_Buffer.GetDeviceAddress().value(),
						.SubmeshMetadataBufferPtr = m_MeshSystem->m_SlotBuffer.GetDeviceAddress().value(),
						.InstanceCount = frameData.MeshCount
					};
					cb.SetPushConstants(pass.GetPushConstantBlock("Constants"), constants);
				}
			});
			
			{
				aZero::RenderAPI::Buffer::TransitionDesc trDesc{
						.DstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
						.DstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
						.Offset = 0,
						.Size = m_MeshDraw_IA_Counter_Buffer.GetDesc().Size
				};
				m_MeshDraw_IA_Counter_Buffer.Transition(trDesc);
			}

			{
				aZero::RenderAPI::Buffer::TransitionDesc trDesc{
						.DstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
						.DstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
						.Offset = 0,
						.Size = m_MeshDraw_IA_Buffer.GetDesc().Size
				};
				m_MeshDraw_IA_Buffer.Transition(trDesc);
			}

			{
				aZero::RenderAPI::Buffer::TransitionDesc trDesc{
						.DstStageMask = VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT,
						.DstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
						.Offset = 0,
						.Size = m_MeshDraw_DrawMetadata_Buffer.GetDesc().Size
				};
				m_MeshDraw_DrawMetadata_Buffer.Transition(trDesc);
			}

			commandBuffer.Barrier({}, {}, { m_MeshDraw_IA_Counter_Buffer.GetTransition(),  m_MeshDraw_IA_Buffer.GetTransition(),  m_MeshDraw_DrawMetadata_Buffer.GetTransition() });
		}

		void RecordMeshDrawPass(RenderAPI::CommandBuffer& commandBuffer, const std::vector<uint32_t>& materialOffsets, const std::vector<uint32_t>& perMaterialCounts, const Scene::SceneRenderData& renderData, const Scene::SceneRenderDataFrameInfo& frameData, uint32_t cameraIndex)
		{
			const Component::CameraComponent& camera = *frameData.Cameras[cameraIndex];
			auto [vp, scizzor] = camera.GetViewportAndScizzorRect();

			auto iter = m_MaterialSystem->m_Materials.begin();
			bool hasRenderedOnce = false;
			for (uint32_t i = 0; i < materialOffsets.size(); i++)
			{
				if (iter == m_MaterialSystem->m_Materials.end())
				{
					return;
				}

				if (perMaterialCounts[i] == 0)
				{
					iter++;
					continue;
				}

				// todo Skip empty ones
				Pipeline::RenderPass& pass = iter->second.Pipeline;
				if (!pass.IsValid())
				{
					iter++;
					continue;
				}
				iter++;

				Pipeline::RenderPass::RenderDesc rDesc{
					.RenderRect = scizzor,
					.DescriptorSets = {&m_TextureSystem->m_DescriptorSet},
					.Callback = [&](RenderAPI::CommandBuffer& cb, Pipeline::RenderPass& pass) {

						// since we made them dynamic when creating the pipeline we overwrite them here
						cb.SetScizzorRectsAndViewports({ vp }, { scizzor });

						aZero::GPUStruct::MeshDrawPushConstants constants{
							 .CameraBufferPtr = renderData.CameraBuffer.GetDeviceAddress().value() + cameraIndex * sizeof(GPUStruct::CameraData),
							 .InstanceDataBufferPtr = renderData.InstanceBuffer.GetDeviceAddress().value(),
							 .DrawMetadataBufferPtr = m_MeshDraw_DrawMetadata_Buffer.GetDeviceAddress().value() + materialOffsets[i] * sizeof(GPUStruct::DrawMetaData),

							 .PointLightsPtr = renderData.PointLightBuffer.GetDeviceAddress().value(),
							 .NumPointLights = frameData.PointLightCount,

							 .SpotLightsPtr = renderData.SpotLightBuffer.GetDeviceAddress().value(),
							 .NumSpotLights = frameData.SpotLightCount,

							 .DirectionalLightsPtr = renderData.DirectionalLightBuffer.GetDeviceAddress().value(),
							 .NumDirectionalLights = frameData.DirectionalLightCount,

							 .RenderMode = 0u
						};
						cb.SetPushConstants(pass.GetPushConstantBlock("Constants"), constants);

						RenderAPI::CommandBuffer::ExecuteIndirectDesc exeDesc{
							.IndirectArgumentBuffer = &m_MeshDraw_IA_Buffer,
							.IndirectArgumentBufferOffset = materialOffsets[i] * sizeof(aZero::GPUStruct::MeshTaskIndirectArgs), // Starts after count
							.IndirectArgumentStride = (uint32_t)(sizeof(aZero::GPUStruct::MeshTaskIndirectArgs)),
							.CountBuffer = &m_MeshDraw_IA_Counter_Buffer,
							.CountBufferOffset = i * sizeof(aZero::GPUStruct::IndirectArgumentCounter),
							.MaxDraws = perMaterialCounts[i] // set to num entities in scene that can be rendered
						};
						cb.ExecuteIndirectMeshTasksCount(exeDesc);
					}
				};

				Pipeline::RenderPass::RenderDesc::ColorTarget cTarget;
				cTarget.LoadOp = camera.m_ClearColorTargetOnRender ? VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR : VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD;
				cTarget.View = camera.m_OptColorTarget.value().get().m_View.Get();

				Pipeline::RenderPass::RenderDesc::DepthStencilTarget dsTarget{
					.View = &camera.m_OptDepthStencilTarget.value().get().m_View,
					.LoadOp = camera.m_ClearDepthStencilTargetOnRender ? VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR : VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD
				};
				if (hasRenderedOnce)
				{
					cTarget.LoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
					dsTarget.LoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				}

				hasRenderedOnce = true;
				rDesc.ColorTargets.push_back(cTarget);
				rDesc.DepthTarget = dsTarget;
				pass.Render(commandBuffer, rDesc);
			}
		}

		void RenderWithCamera(RenderAPI::CommandBuffer& commandBuffer, const std::vector<uint32_t>& materialOffsets, const std::vector<uint32_t>& materialCounts, const Scene::SceneRenderData& renderData, const Scene::SceneRenderDataFrameInfo& frameData, uint32_t cameraIndex)
		{
			this->RecordMeshCullPass(commandBuffer, materialOffsets, renderData, frameData, cameraIndex);
			this->RecordMeshDrawPass(commandBuffer, materialOffsets, materialCounts, renderData, frameData, cameraIndex);
		}

		void RecordFullscreenPass(aZero::Renderer::RenderTarget& srcTarget, aZero::RenderAPI::Swapchain& swapchain, float exposure, ETonemapType tonemap)
		{
			aZero::Renderer::FrameContext& frameContext = m_FrameContexts[m_FrameIndex];
			RenderAPI::CommandBuffer cb = m_CommandManager.GetBuffer();
			cb.StartRecording(VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
			// todo Impl fullscreen pass + tonemap etc

			// Prepares the hdr rtv for shader sampling
			RenderAPI::Image::TransitionDesc srcDesc{
				.DstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				.DstAccessMask = VK_ACCESS_SHADER_READ_BIT,
				.NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.SubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
			};
			srcTarget.m_Image.Transition(srcDesc);
			//

			RenderAPI::Image::TransitionDesc scDesc{
					.DstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
					.DstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
					.NewLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					.SubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
			};

			swapchain.GetCurrentBackBuffer().Transition(scDesc);

			cb.Barrier({}, { swapchain.GetCurrentBackBuffer().GetTransition(), srcTarget.m_Image.GetTransition()}, {});

			// render

			VkViewport vp{
				.x = 0.f,
				.y = 0.f,
				.width = (float)swapchain.m_CreateInfo.imageExtent.width,
				.height = (float)swapchain.m_CreateInfo.imageExtent.height,
				.minDepth = 0.f,
				.maxDepth = 1.f
			};
			VkRect2D scizzor{
				.offset = {.x = 0, .y = 0},
				.extent = {.width = (uint32_t)swapchain.m_CreateInfo.imageExtent.width, .height = (uint32_t)swapchain.m_CreateInfo.imageExtent.height}
			};

			Pipeline::RenderPass::RenderDesc rDesc{
					.RenderRect = scizzor,
					.DescriptorSets = {&m_RenderTargetDescriptorSet},
					.Callback = [&](RenderAPI::CommandBuffer& cb, Pipeline::RenderPass& pass) {

					cb.SetScizzorRectsAndViewports({ vp }, { scizzor });

					struct Constants
					{
						uint32_t HDR_Texture_Index;
						float Exposure;
						uint32_t TonemapType;
					} constants{
						.HDR_Texture_Index = srcTarget.m_SampleDescriptor.Index,
						.Exposure = exposure, // todo Setting
						.TonemapType = (uint32_t)tonemap
					};
					cb.SetPushConstants(pass.GetPushConstantBlock("Constants"), constants);

					VkBuffer vb = m_FullscreenQuadBuffer.Get();
					VkDeviceSize ds{ 0 };
					vkCmdBindVertexBuffers(cb.Get(), 0, 1, &vb, &ds);
					
					RenderAPI::CommandBuffer::DrawDesc dd{
						.VertexCount = 6,
						.InstanceCount = 1,
						.FirstVertex = 0,
						.FirstInstance = 0
					};
					cb.Draw(dd);
				}
			};

			Pipeline::RenderPass::RenderDesc::ColorTarget cTarget;
			cTarget.View = swapchain.GetCurrentBackBufferView().Get();
			rDesc.ColorTargets.push_back(cTarget);
			m_FullscreenPass.Render(cb, rDesc);

			//

			// Prepares the hdr rtv for rendering
			srcDesc = {
				.DstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				.DstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				.NewLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.SubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
			};
			srcTarget.m_Image.Transition(srcDesc);
			//

			scDesc = {
					.DstStageMask = VK_PIPELINE_STAGE_2_NONE,
					.DstAccessMask = VK_ACCESS_2_NONE,
					.NewLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
					.SubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
			};

			swapchain.GetCurrentBackBuffer().Transition(scDesc);

			cb.Barrier({}, { swapchain.GetCurrentBackBuffer().GetTransition(), srcTarget.m_Image.GetTransition() }, {});

			//
			cb.StopRecording();

			aZero::RenderAPI::CommandQueue::SubmitDesc submitDesc{
			   .CommandBuffers = {&cb },
			   // Wait at "acquire image semaphore" when it's time to output to a target, aka in this case the back-buffer in the fullscreen pass.6
			   .WaitOnSemaphores = {{.SemaphorePtr = swapchain.GetImageReadySemaphore(), .Stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}},

			   // Signal when all graphics-pipeline commands are done.
			   .SignalSemaphores = {{.SemaphorePtr = swapchain.GetRenderCompletedSemaphore(), .Stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}, {.SemaphorePtr = &m_RenderSemaphore, .Stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT}}
			};
			m_GraphicsQueue.SubmitCommandBuffers(submitDesc);
		}

		void InitAssetSystemDefaults()
		{
			Asset::ImageData defaultData{
				.BaseWidth = 1,
				.BaseHeight = 1,
				.FaceCount = 1,
				.DepthOrArrayCount = 1,
				.IsArray = false,
				.Format = VkFormat::VK_FORMAT_R8G8B8A8_SRGB,
				.Type = VkImageType::VK_IMAGE_TYPE_2D
			};

			aZero::Asset::ImageData::Mip miplevel;
			miplevel.Offset = 0;
			miplevel.RowPitch = defaultData.BaseWidth * sizeof(DWORD);
			miplevel.SlicePitch = miplevel.RowPitch * defaultData.BaseHeight;

			defaultData.MipLevels.emplace_back(miplevel);

			defaultData.Data.resize(sizeof(uint32_t));
			memcpy(defaultData.Data.data(), (void*)&miplevel.Offset, sizeof(uint32_t));

			m_TextureSystem->CreateTexture(System::TextureSystem::s_Null_TextureStr, defaultData);

			//

			Asset::MaterialDefinition def{
				.Parameters = {
					{"AlbedoTexture", Asset::ParameterType::tex2D},
					{"NormalMap", Asset::ParameterType::tex2D},
					{"RoughnessMetallic", Asset::ParameterType::tex2D},
					{"GlowMap", Asset::ParameterType::tex2D},
					{"TransparencyMap", Asset::ParameterType::tex2D},
					{"RoughnessFactor", Asset::ParameterType::float1},
					{"MetallicFactor", Asset::ParameterType::float1},
				},
				.Properties = 0,
				.MaterialShaderPath = std::string(ENGINE_SHADER_DIRECTORY) + "/PBR_Material.slang",
				.MaterialShaderName = "PBRMaterial"
			};

			auto pbrMaterialRes = m_MaterialSystem->CreateMaterial(System::MaterialSystem::s_PBRTypeStr, def);
			if (pbrMaterialRes)
			{
				auto materialInstance = m_MaterialSystem->CreateMaterialInstance(System::MaterialSystem::s_DefaultPBRInstanceStr, pbrMaterialRes.value().get());
				if (materialInstance)
				{
					auto nullTexture = m_TextureSystem->GetTexture(System::TextureSystem::s_Null_TextureStr);
					if (nullTexture)
					{
						materialInstance->get().SetParameter("AlbedoTexture", nullTexture.value().get().GetDescriptorIndex());
						materialInstance->get().SetParameter("NormalMap", nullTexture.value().get().GetDescriptorIndex());
						materialInstance->get().SetParameter("RoughnessMetallic", nullTexture.value().get().GetDescriptorIndex());
						materialInstance->get().SetParameter("GlowMap", nullTexture.value().get().GetDescriptorIndex());
						materialInstance->get().SetParameter("TransparencyMap", nullTexture.value().get().GetDescriptorIndex());
						materialInstance->get().SetParameter("RoughnessFactor", 0);
						materialInstance->get().SetParameter("MetallicFactor", 0);
					}
				}
			}
		}

		void InitFullscreenQuad()
		{
			struct FullscreenQuadVertex
			{
				glm::vec4 Position;
				glm::vec2 UV;
			};

			std::array<FullscreenQuadVertex, 6> fullscreenVertices{
	{
		{.Position = {-1.f,  1.f, 0.f, 1.f}, .UV = {0.f, 1.f}},
		{.Position = { 1.f,  1.f, 0.f, 1.f}, .UV = {1.f, 1.f}},
		{.Position = {-1.f, -1.f, 0.f, 1.f}, .UV = {0.f, 0.f}},

		{.Position = { 1.f,  1.f, 0.f, 1.f}, .UV = {1.f, 1.f}},
		{.Position = { 1.f, -1.f, 0.f, 1.f}, .UV = {1.f, 0.f}},
		{.Position = {-1.f, -1.f, 0.f, 1.f}, .UV = {0.f, 0.f}}
	}
			};

			RenderAPI::Buffer::Desc desc{
				.Size = fullscreenVertices.size() * sizeof(FullscreenQuadVertex),
				.UsageBits =
					VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				.MemoryType = RenderAPI::EMemoryType::VRAM,
				.SeperateHeap = false,
				.Name = "m_FullscreenQuadBuffer",
				.OptResourceRecycler = *m_ResourceRecycler.get()
			};

			m_FullscreenQuadBuffer = RenderAPI::Buffer(*m_diCore, desc);

			aZero::Renderer::FrameContext& frameContext = m_FrameContexts[m_FrameIndex];
			frameContext.GetFrameAllocator().Allocate(fullscreenVertices.data(), fullscreenVertices.size() * sizeof(FullscreenQuadVertex), m_FullscreenQuadBuffer, 0);

			Compiler::CompilerSession::Desc sessionDesc{
				.IncludePaths = std::vector<std::filesystem::path>(
				{
				}
				),
				.CompilerOptions = std::vector<slang::CompilerOptionEntry>({
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
					{slang::CompilerOptionValueKind::Int, SLANG_OPTIMIZATION_LEVEL_NONE} // todo Change based on build
				}
				})
			};

			m_Session = m_diCompiler->CreateSession(sessionDesc).value();

			auto res = m_Session.LoadModule(std::string(ENGINE_SHADER_DIRECTORY) + "/core_shaders/hdr_to_ldr/HDR_To_LDR.slang");
			if (res)
			{
				Compiler::ShaderProgram::EntryPointDesc eDesc{ .Name = "main" };
				Compiler::ShaderProgram::VertexDesc vsProgramDesc{
					.EntryPoints = {{"vsMain"}, {"fsMain"}},
					.Modules = { &res.value()}
				};

				Compiler::ShaderProgram program;
				program.Build(vsProgramDesc);

				Pipeline::RenderPass pass(*m_diCore);
				Pipeline::RenderPass::VertexPassDesc vsPassDesc{};
				vsPassDesc.Program = &program;
				vsPassDesc.RenderTargetFormats = { VkFormat::VK_FORMAT_R8G8B8A8_SRGB };
				vsPassDesc.VertexBindings =
				{
					{
						.Stride = sizeof(FullscreenQuadVertex),
						.InputRate = VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX,
						.AttributeFormats = {
							VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT,
							VkFormat::VK_FORMAT_R32G32_SFLOAT
						}
					}
				};

				if (pass.Compile(vsPassDesc))
				{
					m_FullscreenPass = std::move(pass);
				}
			}
		}

		void InitRenderTargetDescriptorPool()
		{
			RenderAPI::DescriptorSet::Desc setDesc{ .Type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
				.MaxDescriptorCount = m_diCore->GetVariadicDescriptorCount(), .DescriptorCount = 1000, .ShaderStages = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT };
			RenderAPI::DescriptorPool::Desc descriptorPoolDesc{
				.MaxSets = 1,
				.Sets {setDesc}
			};
			m_RenderTargetDescriptorPool.Init(*m_diCore, descriptorPoolDesc);
			m_RenderTargetDescriptorSet = m_RenderTargetDescriptorPool.CreateDescriptorSet(setDesc);
		}

		void TestInit()
		{
			static constexpr auto supportedExtensions =
				fastgltf::Extensions::KHR_mesh_quantization |
				fastgltf::Extensions::KHR_texture_transform |
				fastgltf::Extensions::KHR_materials_variants |
				fastgltf::Extensions::KHR_lights_punctual |
				fastgltf::Extensions::KHR_texture_basisu
				;

			fastgltf::Parser parser(supportedExtensions);

			constexpr auto gltfOptions =
				fastgltf::Options::DontRequireValidAssetMember |
				fastgltf::Options::AllowDouble |
				fastgltf::Options::LoadExternalBuffers |
				//fastgltf::Options::LoadExternalImages |
				fastgltf::Options::GenerateMeshIndices |
				fastgltf::Options::DecomposeNodeMatrices
				;

			const std::filesystem::path path(std::string(PROJECT_DIRECTORY) + "KtxCube.gltf");
			auto gltfFile = fastgltf::MappedGltfFile::FromPath(path);
			if (!bool(gltfFile)) {
				std::cerr << "Failed to open glTF file: " << fastgltf::getErrorMessage(gltfFile.error()) << '\n';
				throw;
			}

			auto loadedAsset = parser.loadGltf(gltfFile.get(), path.parent_path(), gltfOptions);
			fastgltf::Asset* asset = loadedAsset.get_if();
			if (!asset) {
				std::cerr << "Failed to load glTF file: " << fastgltf::getErrorMessage(loadedAsset.error()) << '\n';
				throw;
			}

			auto& meshSystem = *m_MeshSystem.get();
			auto& textureSystem = *m_TextureSystem.get();
			auto& materialSystem = *m_MaterialSystem.get();

			{
				auto imageData = aZero::Asset::ExtractGltfImage(asset->images[0], asset, path);
				if (imageData)
				{
					auto texture = textureSystem.CreateTexture("MyTexture", imageData.value());
					if (texture)
					{
						Asset::MaterialDefinition def{
						.Parameters = {
							{"float1", Asset::ParameterType::float1},
							{"int3", Asset::ParameterType::int3},
							{"texture", Asset::ParameterType::tex2D}
						},
						.Properties = 0,
						.MaterialShaderPath = std::string(ENGINE_SHADER_DIRECTORY) + "/testMaterial.slang",
						.MaterialShaderName = "TestMaterial2"
						};

						auto material = materialSystem.CreateMaterial("MyMaterialType", def);
						if (material)
						{
							auto materialInstance = materialSystem.CreateMaterialInstance("MyMaterialInstance", material->get());
							if (materialInstance)
							{
								materialInstance->get().SetParameter("float1", 1.f);
								materialInstance->get().SetParameter("int3", glm::ivec3(1, 2, 3));
								materialInstance->get().SetParameter("texture", texture->get().GetDescriptorIndex());

								testMaterialPtr = materialInstance->get().GetBDA();

								Asset::MeshletMeshData meshData = aZero::Asset::ExtractGltfMeshletMesh(asset->meshes[0], asset);
								auto mesh = meshSystem.CreateMesh("MyMesh", meshData);
								if (mesh)
								{
									// todo Create test entity that will get rendered
									int x = 2;
								}
							}
						}
					}
				}
			}
		}

		bool m_HasSignalledFrameComplete = false;
		RenderAPI::Core* m_diCore = nullptr;
		Compiler::Compiler* m_diCompiler = nullptr;
		std::unique_ptr<aZero::Renderer::ResourceRecycler> m_ResourceRecycler;
		aZero::Renderer::CommandManager m_CommandManager;
		std::vector<aZero::Renderer::FrameContext> m_FrameContexts;
		RenderAPI::CommandQueue m_GraphicsQueue;
		uint32_t m_FrameIndex;
		RenderAPI::Semaphore m_RenderSemaphore;
		RenderAPI::DescriptorPool m_RenderTargetDescriptorPool;
		RenderAPI::DescriptorSet m_RenderTargetDescriptorSet;
		aZero::DS::IndexFreelist<uint32_t> m_SampledRenderTargetFreelist;

		Compiler::CompilerSession m_Session;
		Pipeline::RenderPass m_ComputeCullPass;
		Pipeline::RenderPass m_FullscreenPass;

		RenderAPI::CommandBuffer m_FrameDataUploader;
		std::unique_ptr<aZero::System::MeshSystem> m_MeshSystem;
		std::unique_ptr<aZero::System::TextureSystem> m_TextureSystem;
		std::unique_ptr<aZero::System::MaterialSystem> m_MaterialSystem;

		RenderAPI::Buffer m_MeshDraw_IA_Buffer;
		RenderAPI::Buffer m_MeshDraw_IA_Counter_Buffer;
		RenderAPI::Buffer m_MeshDraw_DrawMetadata_Buffer;
		RenderAPI::Buffer m_FullscreenQuadBuffer;
	};
}