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
			m_MaterialSystem = std::make_unique<System::MaterialSystem>(*m_diCore, compiler, *m_ResourceRecycler, (uint64_t)(2048 * 2048), 1000);


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
			this->TestInit();
			this->RebuildComputePipeline();
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
					{slang::CompilerOptionValueKind::Int, SLANG_OPTIMIZATION_LEVEL_NONE}
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

			m_MeshSystem->NextFrame(m_FrameDataUploader);
			m_TextureSystem->NextFrame(m_FrameIndex, m_FrameDataUploader);

			return true;
		}

		void FinalizeFrame()
		{
			aZero::Renderer::FrameContext& frameContext = m_FrameContexts[m_FrameIndex];

			m_TextureSystem->UpdateDirtyTextures();
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
				.DstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
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
			if (!rTarget.Init(type, dimensions, *m_diCore, *m_ResourceRecycler))
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
			}
			return rTarget;
		}

		void RenderScene(Scene::Scene& scene)
		{
			auto materialCounts = m_MaterialSystem->CreateVectorForMaterialTypeCount();
			if (materialCounts.empty())
			{
				return;
			}

			aZero::Renderer::FrameContext& frameContext = m_FrameContexts[m_FrameIndex];
			RenderAPI::CommandBuffer commandBuffer = m_CommandManager.GetBuffer();
			commandBuffer.StartRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

			std::vector<uint32_t> materialOffsets(materialCounts.size());
			std::exclusive_scan(materialCounts.cbegin(), materialCounts.cend(), materialOffsets.begin(), 0);

			auto [frameData, renderData] = scene.CreateRenderData(*m_diCore, *m_ResourceRecycler.get(), frameContext.GetFrameAllocator().GetAllocator(), frameContext.GetFrameAllocator().GetBuffer(), commandBuffer);
			for (uint32_t i = 0; i < frameData.Cameras.size(); i++) // todo For each camera
			{
				const Component::CameraComponent& camera = *frameData.Cameras[i];
				if (camera.IsValidForRendering())
				{
					this->RenderWithCamera(commandBuffer, materialOffsets, materialCounts, renderData, frameData, i);
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

		uint32_t GetFrameIndex() const { return m_FrameIndex; }
		FrameContext& GetCurrentContext() { return m_FrameContexts.at(m_FrameIndex); }
		uint32_t GetFramesInFlight() const { return m_FrameContexts.size(); }
		void FlushRenderingCommands() const { m_diCore->FlushGPU(); }

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
						.InstanceCullDataBufferPtr = renderData.InstanceCullDataBuffer.GetDeviceAddress().value(),
						.DrawMetadataBufferPtr = m_MeshDraw_DrawMetadata_Buffer.GetDeviceAddress().value(),
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

		void RecordMeshDrawPass(RenderAPI::CommandBuffer& commandBuffer, const std::vector<uint32_t>& materialOffsets, const std::vector<uint32_t>& materialCounts, const Scene::SceneRenderData& renderData, const Scene::SceneRenderDataFrameInfo& frameData, uint32_t cameraIndex)
		{
			const Component::CameraComponent& camera = *frameData.Cameras[cameraIndex];
			auto [vp, scizzor] = camera.GetViewportAndScizzorRect();
			for (uint32_t i = 0; i < materialOffsets.size(); i++)
			{
				Pipeline::RenderPass& pass = m_MaterialSystem->m_Passes[i];

				Pipeline::RenderPass::RenderDesc rDesc{
					.RenderRect = scizzor,
					.DescriptorSets = {/*&m_TextureSystem->m_DescriptorSet*/},
					.Callback = [&](RenderAPI::CommandBuffer& cb, Pipeline::RenderPass& pass) {

						// since we made them dynamic when creating the pipeline we overwrite them here
						cb.SetScizzorRectsAndViewports({ vp }, { scizzor });

						aZero::GPUStruct::MeshDrawPushConstants constants{
							 .CameraBufferPtr = renderData.CameraBuffer.GetDeviceAddress().value() + cameraIndex * sizeof(GPUStruct::CameraData),
							 .InstanceDataBufferPtr = renderData.InstanceBuffer.GetDeviceAddress().value(),
							 .DrawMetadataBufferPtr = m_MeshDraw_DrawMetadata_Buffer.GetDeviceAddress().value() + (uint32_t)materialOffsets[i] * sizeof(GPUStruct::DrawMetaData),

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
							.IndirectArgumentBufferOffset = (uint32_t)materialOffsets[i] * sizeof(aZero::GPUStruct::MeshTaskIndirectArgs), // Starts after count
							.IndirectArgumentStride = (uint32_t)(sizeof(aZero::GPUStruct::MeshTaskIndirectArgs)),
							.CountBuffer = &m_MeshDraw_IA_Counter_Buffer,
							.CountBufferOffset = i * sizeof(aZero::GPUStruct::IndirectArgumentCounter),
							.MaxDraws = (uint32_t)(m_MeshDraw_IA_Buffer.GetDesc().Size / sizeof(aZero::GPUStruct::MeshTaskIndirectArgs)) // set to num entities in scene that can be rendered
						};
						cb.ExecuteIndirectMeshTasksCount(exeDesc);
					}
				};

				Pipeline::RenderPass::RenderDesc::ColorTarget cTarget;
				cTarget.LoadOp = camera.m_ClearColorTargetOnRender ? VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR : VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD;
				cTarget.View = camera.m_OptColorTarget.value().get().m_View.Get();
				rDesc.ColorTargets.push_back(cTarget);

				Pipeline::RenderPass::RenderDesc::DepthStencilTarget dsTarget{
					.View = &camera.m_OptDepthStencilTarget.value().get().m_View,
					.LoadOp = camera.m_ClearDepthStencilTargetOnRender ? VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR : VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD
				};
				rDesc.DepthTarget = dsTarget;
				pass.Render(commandBuffer, rDesc);
			}
		}

		void RenderWithCamera(RenderAPI::CommandBuffer& commandBuffer, const std::vector<uint32_t>& materialOffsets, const std::vector<uint32_t>& materialCounts, const Scene::SceneRenderData& renderData, const Scene::SceneRenderDataFrameInfo& frameData, uint32_t cameraIndex)
		{
			this->RecordMeshCullPass(commandBuffer, materialOffsets, renderData, frameData, cameraIndex);
			this->RecordMeshDrawPass(commandBuffer, materialOffsets, materialCounts, renderData, frameData, cameraIndex);
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

			Asset::ImageData imageData = aZero::File::ExtractGltfImage(asset->images[0], asset, path);
			auto texture = textureSystem.CreateTexture("MyTexture", imageData);
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
						materialInstance->get().SetParameter("float1", 6.7f);
						materialInstance->get().SetParameter("int3", glm::ivec3(1, 2, 3));
						materialInstance->get().SetParameter("texture", texture->get().GetDescriptorIndex());

						testMaterialPtr = materialInstance->get().GetBDA();

						Asset::MeshletMeshData meshData = aZero::File::ExtractGltfMeshletMesh(asset->meshes[0], asset);
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

		bool m_HasSignalledFrameComplete = false;
		RenderAPI::Core* m_diCore = nullptr;
		Compiler::Compiler* m_diCompiler = nullptr;
		std::unique_ptr<aZero::Renderer::ResourceRecycler> m_ResourceRecycler;
		aZero::Renderer::CommandManager m_CommandManager;
		std::vector<aZero::Renderer::FrameContext> m_FrameContexts;
		RenderAPI::CommandQueue m_GraphicsQueue;
		uint32_t m_FrameIndex;
		RenderAPI::Semaphore m_RenderSemaphore;

		Compiler::CompilerSession m_Session;
		Pipeline::RenderPass m_ComputeCullPass;

		RenderAPI::CommandBuffer m_FrameDataUploader;
		std::unique_ptr<aZero::System::MeshSystem> m_MeshSystem;
		std::unique_ptr<aZero::System::TextureSystem> m_TextureSystem;
		std::unique_ptr<aZero::System::MaterialSystem> m_MaterialSystem;

		RenderAPI::Buffer m_MeshDraw_IA_Buffer;
		RenderAPI::Buffer m_MeshDraw_IA_Counter_Buffer;
		RenderAPI::Buffer m_MeshDraw_DrawMetadata_Buffer;
	};
}