#pragma once
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <numeric>
#include "asset/material/MaterialInstance.hpp"
#include "misc/IndexFreelist.hpp"
#include "misc/FreelistAllocator.hpp"
#include "renderer/FrameAllocator.hpp"
#include "render_api/Compiler.hpp"

namespace aZero::Renderer { class Renderer; }

namespace aZero::System
{
	class MaterialSystem
	{
		friend class aZero::Renderer::Renderer;

		struct MaterialAllocationInfo
		{
			Asset::MaterialType MaterialBase;
			Compiler::ShaderProgram ShaderProgram;
		};

		struct MaterialInstanceAllocationInfo
		{
			aZero::FreelistAllocator::Allocation Allocation;
			Asset::MaterialInstance Instance;
		};
	public:
		using MaterialIndirectCounter = uint32_t;

		MaterialSystem() = default;
		MaterialSystem(RenderAPI::Core& core, Compiler::Compiler& diCompiler, Renderer::ResourceRecycler& recycler, uint64_t bufferSize/*, uint32_t framesInFlight*/, uint32_t maxInitialMaterials)
			:m_diCore(&core), m_diCompiler(&diCompiler), m_Allocator(aZero::FreelistAllocator(bufferSize))
		{
			//m_DeferredAllocationHandler.resize(framesInFlight);
			{
				const RenderAPI::Buffer::Desc desc{
					.Size = bufferSize,
					.UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					.MemoryType = RenderAPI::EMemoryType::VRAM,
					.SeperateHeap = true,
					.OptResourceRecycler = recycler
				};
				m_MaterialInstanceBuffer.Init(core, desc);
			}

			{
				const RenderAPI::Buffer::Desc desc{
					.Size = maxInitialMaterials * sizeof(MaterialIndirectCounter),
					.UsageBits = 
						VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT 
						| VkBufferUsageFlagBits::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT 
						| VkBufferUsageFlagBits::VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT // For use with cmdbuffer ia draw
						| VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT, // For use as RW in cull shader
					.MemoryType = RenderAPI::EMemoryType::VRAM,
					.SeperateHeap = false,
					.OptResourceRecycler = recycler
				};
				m_MaterialInstanceCounterBuffer.Init(core, desc);
			}

			this->ReloadShaderModules();
		}

		void ReloadShaderModules()
		{
			Compiler::CompilerSession::Desc sessionDesc {
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
			m_TaskShader = m_Session.LoadModule(std::string(ENGINE_SHADER_DIRECTORY) + "/core_shaders/Geo_TaskShader.slang").value();
			m_MeshShader = m_Session.LoadModule(std::string(ENGINE_SHADER_DIRECTORY) + "/core_shaders/Geo_MeshShader.slang").value();
			m_FragmentShader = m_Session.LoadModule(std::string(ENGINE_SHADER_DIRECTORY) + "/core_shaders/Geo_FragmentShader.slang").value();
		}

		std::optional<std::reference_wrapper<Asset::MaterialType>> CreateMaterial(const Asset::MaterialDefinition& definition)
		{
			Asset::MaterialRuntimeTypeID newMaterialTypeIndex = m_MaterialTypeIndexList.New();
			MaterialAllocationInfo info{
				.MaterialBase = Asset::MaterialType(definition, newMaterialTypeIndex)
			};
			// Copy the name since we might remove it with DestroyMaterial which makes "definition" dangling
			const std::string name = info.MaterialBase.GetDefinition().Name;

			Pipeline::RenderPass pass(*m_diCore);
			if (!this->CreatePipeline(info, pass))
			{
				m_MaterialTypeIndexList.Recycle(newMaterialTypeIndex);
				return std::nullopt;
			}

			if (auto iter = m_Materials.find(name); iter != m_Materials.end())
			{
				this->DestroyMaterial(iter->second.MaterialBase);
			}

			if (m_Passes.size() <= newMaterialTypeIndex)
			{
				m_Passes.emplace_back(std::move(pass));
			}
			else
			{
				m_Passes[newMaterialTypeIndex] = std::move(pass);
			}

			auto [it, inserted] = m_Materials.emplace(name, std::move(info));
			return it->second.MaterialBase;
		}

		std::optional<std::reference_wrapper<Asset::MaterialInstance>> CreateMaterialInstance(const std::string& name, const Asset::MaterialType& material)
		{
			if (m_MaterialInstances.contains(name))
			{
				return std::nullopt;
			}

			if (!m_Allocator.CanAllocate(material.GetParameterSizeInfo().TotalSize))
			{
				// todo Handle if material buffer is full
			}

			auto allocation = m_Allocator.Allocate(material.GetParameterSizeInfo().TotalSize);
			Asset::MaterialInstanceBDA instanceBDA = static_cast<Asset::MaterialInstanceBDA>(m_MaterialInstanceBuffer.GetDeviceAddress().value() + allocation.Offset);

			auto [it, inserted] = m_MaterialInstances.emplace(
				name,
				MaterialInstanceAllocationInfo{
					.Allocation = allocation,
					.Instance = Asset::MaterialInstance(material, name, instanceBDA)
				});
			return it->second.Instance;
		}

		void DestroyMaterial(const Asset::MaterialType& material)
		{
			if (auto iter = m_Materials.find(material.GetDefinition().Name); iter != m_Materials.end())
			{
				if (iter->second.MaterialBase.GetRuntimeTypeID() == material.GetRuntimeTypeID())
				{
					std::erase_if(m_MaterialInstances, [&](auto& kv) {
						if (kv.second.Instance.m_diMaterial->m_RuntimeTypeID == iter->second.MaterialBase.GetRuntimeTypeID())
						{
							//m_DeferredAllocationHandler[m_FrameIndex].push_back(kv.second.Allocation);
							m_Allocator.Free(kv.second.Allocation);
							kv.second.Instance.m_InstanceBDA = Asset::g_InvalidInstanceBDA;
							// todo Handle references to the instance
							return true;
						}
						return false;
					});
					m_MaterialTypeIndexList.Recycle(iter->second.MaterialBase.GetRuntimeTypeID());
					m_Materials.erase(iter);
				}
			}
		}

		void DestroyMaterialInstance(const Asset::MaterialInstance& instance)
		{
			if (auto iter = m_MaterialInstances.find(instance.GetName()); iter != m_MaterialInstances.end())
			{
				if (iter->second.Instance.m_InstanceBDA == instance.m_InstanceBDA)
				{
					//m_DeferredAllocationHandler[m_FrameIndex].push_back(iter->second.Allocation);
					m_Allocator.Free(iter->second.Allocation);
					m_MaterialInstances.erase(iter);

					// todo Handle references to the instance
				}
			}
		}

		std::optional<std::reference_wrapper<Asset::MaterialType>> GetMaterial(const std::string& name)
		{
			if (auto iter = m_Materials.find(name); iter != m_Materials.end())
			{
				return iter->second.MaterialBase;
			}
			return std::nullopt;
		}

		std::optional<std::reference_wrapper<Asset::MaterialInstance>> GetMaterialInstance(const std::string& name)
		{
			if (auto iter = m_MaterialInstances.find(name); iter != m_MaterialInstances.end())
			{
				return iter->second.Instance;
			}
			return std::nullopt;
		}

		const RenderAPI::Buffer& GetMaterialInstanceBuffer() const { return m_MaterialInstanceBuffer; }
		const RenderAPI::Buffer& GetMaterialInstanceCounterBuffer() const { return m_MaterialInstanceCounterBuffer; }

		// Allocate empty vector with zeroed elements for use with counting num per material type when rendering a scene etc
		std::vector<MaterialIndirectCounter> CreateVectorForMaterialTypeCount() const {
			return std::vector<MaterialIndirectCounter>(m_MaterialTypeIndexList.GetMax(), 0);
		}

		void PrepareUpdateAtomicCounterBuffer(Renderer::FrameAllocator& allocator, const std::vector<MaterialIndirectCounter>& counts)
		{
			if (counts.size() > m_MaterialInstanceCounterBuffer.GetDesc().Size / sizeof(MaterialIndirectCounter))
			{
				// todo Handle if counts exceed the buffer size
				throw;
			}

			std::vector<MaterialIndirectCounter> offsets(counts.size());
			std::exclusive_scan(counts.cbegin(), counts.cend(), offsets.begin(), 0);
			allocator.Allocate(offsets, m_MaterialInstanceCounterBuffer, 0);
		}

		void RecordPasses(RenderAPI::CommandBuffer& commandBuffer)
		{
			// todo Handle order for ex. transparent materials
			for (uint32_t i = 0; i < m_Passes.size(); i++)
			{
				auto& pass = m_Passes[i];
				// todo Sort cameras by order and by color and depth targets
				// todo For each camera in the scene with different color and/or depth targets
				pass.Render(commandBuffer,
					Pipeline::RenderPass::RenderDesc{
						/*.ColorTargets=camera color target,
						.DepthTarget=if depth -> get camera depth stencil,
						.RenderWidth=camera viewport etc,
						.RenderHeight=camera viewport etc,
						.DescriptorSets= texture set,*/
						.Callback = [&](RenderAPI::CommandBuffer& commandBuffer, aZero::Pipeline::RenderPass& pass) {

							// todo For each camera in the list of cameras with the same targets
							/*VkViewport vp{
								.x = 0.f,
								.y = 0.f,
								.width = static_cast<float>(window.GetClientDimensions()[0]),
								.height = static_cast<float>(window.GetClientDimensions()[1]),
								.minDepth = 0.0f,
								.maxDepth = 1.0f
							};
							VkRect2D scissor{.extent{.width = static_cast<uint32_t>(window.GetClientDimensions()[0]), .height = static_cast<uint32_t>(window.GetClientDimensions()[1]) } };
							cb.SetScizzorRectsAndViewports({ vp }, { scissor });*/

							/*aZero::Rendering::GeoDrawPushConstants constants{
								 .CameraBufferPtr = vars.cameraBuffer.GetDeviceAddress().value() + frameIndex * sizeof(aZero::Rendering::CameraData),
								 .MeshletBoundBufferPtr = vars.meshletBoundsBuffer.GetDeviceAddress().value(),
								 .MaterialBufferPtr = vars.materialBuffer.GetDeviceAddress().value(),
								 .InstanceDataBufferPtr = vars.instanceDataBuffer.GetDeviceAddress().value(),
								 .DrawMetadataBufferPtr = vars.drawMetadataBuffer.GetDeviceAddress().value(),
								 .RenderMode = 0u
							};
							cb.SetPushConstants(pass.GetPushConstantBlock("Constants"), constants);*/

							RenderAPI::CommandBuffer::ExecuteIndirectDesc exeDesc{
								//.IndirectArgumentBuffer = &vars.iaBuffer,
								//.IndirectArgumentBufferOffset = (uint32_t)(sizeof(aZero::Rendering::IndirectArgumentCounter)), // Starts after count
								//.IndirectArgumentStride = (uint32_t)(sizeof(uint32_t) * 3),
								.CountBuffer = &m_MaterialInstanceCounterBuffer,
								.CountBufferOffset = (uint32_t)(sizeof(MaterialIndirectCounter) * i),
								//.MaxDraws = 2
							};
							commandBuffer.ExecuteIndirectMeshTasksCount(exeDesc);
						}
					}
				);
			}
		}

	private:
		bool CreatePipeline(MaterialAllocationInfo& allocationInfo, Pipeline::RenderPass& inoutPass)
		{
			Compiler::ShaderProgram program;

			std::optional<Compiler::ShaderModule> optMaterialModule;

			Compiler::ShaderProgram::MeshletDesc msProgramDesc{
				.EntryPoints = {{"asMain"}, {"msMain"},{"psMain", {allocationInfo.MaterialBase.GetDefinition().MaterialShaderName}}},
				.Modules = {&m_TaskShader, &m_MeshShader, &m_FragmentShader}
			};

			if (!allocationInfo.MaterialBase.GetDefinition().MaterialShaderPath.empty())
			{
				optMaterialModule = m_Session.LoadModule(std::string(ENGINE_SHADER_DIRECTORY) + allocationInfo.MaterialBase.GetDefinition().MaterialShaderPath.string());
				if (optMaterialModule)
				{
					msProgramDesc.Modules.push_back(&optMaterialModule.value());
				}
			}

			if (!program.Build(msProgramDesc))
			{
				return false;
			}

			Pipeline::RenderPass::MeshShaderPassDesc mspDesc;
			mspDesc.Program = &program;
			mspDesc.DepthDesc = Pipeline::RenderPass::Desc::DepthStencil();
			mspDesc.RenderTargetFormats = { VK_FORMAT_R8G8B8A8_SRGB };
			mspDesc.RasterDesc.CullMode = VkCullModeFlagBits::VK_CULL_MODE_NONE;
			if (!inoutPass.Compile(mspDesc))
			{
				return false;
			}

			allocationInfo.ShaderProgram = std::move(program);

			return true;
		}

		//void NextFrame(/*uint32_t frameIndex, */Renderer::FrameAllocator& allocator)
		//{
		//	m_diFrameAllocator = &allocator;
		//	/*m_FrameIndex = frameIndex;
		//	if (frameIndex > m_DeferredAllocationHandler.size())
		//	{
		//		throw;
		//	}

		//	for (const auto& allocation : m_DeferredAllocationHandler[frameIndex])
		//	{
		//		m_Allocator.Free(allocation);
		//	}*/
		//}

		void UpdateDirtyMaterials(Renderer::FrameAllocator& allocator)
		{
			for (auto& [name, materialInstance] : m_MaterialInstances) // Horrible, but good for now...
			{
				if (materialInstance.Instance.IsDirty())
				{
					allocator.Allocate(materialInstance.Instance.m_Data, m_MaterialInstanceBuffer, materialInstance.Allocation.Offset);
					materialInstance.Instance.SetNotDirty();
				}
			}
		}

		RenderAPI::Core* m_diCore = nullptr;
		RenderAPI::Buffer m_MaterialInstanceBuffer;
		RenderAPI::Buffer m_MaterialInstanceCounterBuffer;
		aZero::FreelistAllocator m_Allocator;
		aZero::DS::IndexFreelist<Asset::MaterialRuntimeTypeID> m_MaterialTypeIndexList;
		std::unordered_map<std::string, MaterialAllocationInfo> m_Materials;
		std::vector<Pipeline::RenderPass> m_Passes;
		std::unordered_map<std::string, MaterialInstanceAllocationInfo> m_MaterialInstances;
		//std::vector<std::vector<aZero::FreelistAllocator::Allocation>> m_DeferredAllocationHandler;
		//Renderer::FrameAllocator* m_diFrameAllocator = nullptr;
		uint32_t m_FrameIndex;
		Compiler::Compiler* m_diCompiler = nullptr;
		Compiler::CompilerSession m_Session;
		Compiler::ShaderModule m_TaskShader;
		Compiler::ShaderModule m_MeshShader;
		Compiler::ShaderModule m_FragmentShader;
	};
}
