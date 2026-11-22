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
#include "render_api/RenderPass.hpp"
#include "TextureSystem.hpp"

namespace aZero::Renderer { class Renderer; }

namespace aZero::System
{
	class MaterialSystem
	{
		friend class aZero::Renderer::Renderer;

		struct MaterialAllocationInfo
		{
			Asset::MaterialType MaterialBase;
			Pipeline::RenderPass Pipeline;
		};

		struct MaterialInstanceAllocationInfo
		{
			aZero::FreelistAllocator::Allocation Allocation;
			Asset::MaterialInstance Instance;
		};
	public:
		static inline const char* s_PBRTypeStr = "PBR";
		static inline const char* s_DefaultPBRInstanceStr = "Default_PBR";
		MaterialSystem() = default;
		MaterialSystem(RenderAPI::Core& core, Compiler::Compiler& diCompiler, Renderer::ResourceRecycler& recycler, uint64_t bufferSize, uint32_t maxInitialMaterials, System::TextureSystem& textureSystem)
			:m_diCore(&core), m_diCompiler(&diCompiler), m_Allocator(aZero::FreelistAllocator(bufferSize))
		{
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
					.Size = maxInitialMaterials * sizeof(uint32_t),
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
			auto resTS = m_Session.LoadModule(std::string(ENGINE_SHADER_DIRECTORY) + "/core_shaders/mesh_draw_pipeline/TaskShader.slang");
			auto resMS = m_Session.LoadModule(std::string(ENGINE_SHADER_DIRECTORY) + "/core_shaders/mesh_draw_pipeline/MeshShader.slang");
			auto resFS = m_Session.LoadModule(std::string(ENGINE_SHADER_DIRECTORY) + "/core_shaders/mesh_draw_pipeline/FragmentShader.slang");

			if (resTS.has_value() && resMS.has_value() && resFS.has_value())
			{
				// todo Recompile materials and if one fails we exit. Else ->

				m_TaskShader = resTS.value();
				m_MeshShader = resMS.value();
				m_FragmentShader = resFS.value();
			}
		}

		std::optional<std::reference_wrapper<Asset::MaterialType>> CreateMaterial(const std::string& name, const Asset::MaterialDefinition& definition)
		{
			if (m_Materials.contains(name) || !definition.IsValid())
			{
				return std::nullopt;
			}

			Asset::MaterialRuntimeTypeID newMaterialTypeIndex = m_MaterialTypeIndexList.New();
			MaterialAllocationInfo info{
				.MaterialBase = Asset::MaterialType(definition, newMaterialTypeIndex)
			};

			info.MaterialBase.Name = name;

			Pipeline::RenderPass pass(*m_diCore);
			if (!this->CreatePipeline(info, pass))
			{
				m_MaterialTypeIndexList.Recycle(newMaterialTypeIndex);
				return std::nullopt;
			}
			info.Pipeline = std::move(pass);

			auto [it, inserted] = m_Materials.emplace(name, std::move(info));
			return it->second.MaterialBase;
		}

		// todo Support initiation values
		std::optional<std::reference_wrapper<Asset::MaterialInstance>> CreateMaterialInstance(const std::string& name, Asset::MaterialType& material)
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
					.Instance = Asset::MaterialInstance(material, name, instanceBDA, *this)
				});
			return it->second.Instance;
		}

		void DestroyMaterial(const Asset::MaterialType& material)
		{
			if (auto iter = m_Materials.find(material.Name); iter != m_Materials.end())
			{
				if (iter->second.MaterialBase.GetRuntimeTypeID() == material.GetRuntimeTypeID())
				{
					std::erase_if(m_MaterialInstances, [&](auto& kv) {
						if (kv.second.Instance.m_diMaterial->m_RuntimeTypeID == iter->second.MaterialBase.GetRuntimeTypeID())
						{
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
					m_DirtyInstances.erase(&instance);
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

		// Returns a vector with the count per-material-type.
		// The vector order matches the m_Materials iteration order as long as m_Materials iterator hasnt been changed.
		std::vector<uint32_t> GetMaterialTypeCounts() const
		{
			std::vector<uint32_t> counts;
			for (auto& [name, material] : m_Materials) // Horrible, but good for now... As long as we dont have a bajillion unique material types...
			{
				counts.push_back(material.MaterialBase.m_Refcount);
			}
			return counts;
		}

		void SetMaterialInstanceDirty(const Asset::MaterialInstance& instance)
		{
			m_DirtyInstances.emplace(&instance);
		}
		
	private:
		const RenderAPI::Buffer& GetMaterialInstanceBuffer() const { return m_MaterialInstanceBuffer; }
		const RenderAPI::Buffer& GetMaterialInstanceCounterBuffer() const { return m_MaterialInstanceCounterBuffer; }

		// Allocate empty vector with zeroed elements for use with counting num per material type when rendering a scene etc
		std::vector<uint32_t> CreateVectorForMaterialTypeCount() const {
			return std::vector<uint32_t>(m_MaterialTypeIndexList.GetMax(), 0);
		}

		bool CreatePipeline(MaterialAllocationInfo& allocationInfo, Pipeline::RenderPass& inoutPass)
		{
			Compiler::ShaderProgram program;

			std::optional<Compiler::ShaderModule> optMaterialModule;

			Compiler::ShaderProgram::MeshletDesc msProgramDesc{
				.EntryPoints = {{"asMain"}, {"msMain"},{"fsMain", {allocationInfo.MaterialBase.GetDefinition().MaterialShaderName}}},
				.Modules = {&m_TaskShader, &m_MeshShader, &m_FragmentShader}
			};

			if (!allocationInfo.MaterialBase.GetDefinition().MaterialShaderPath.empty())
			{
				optMaterialModule = m_Session.LoadModule(allocationInfo.MaterialBase.GetDefinition().MaterialShaderPath.string());
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
			mspDesc.RenderTargetFormats = { /*VkFormat::VK_FORMAT_R8G8B8A8_SRGB*/VK_FORMAT_R16G16B16A16_SFLOAT /*HDR format*/}; // todo Test VK_FORMAT_B10G11R11_UFLOAT_PACK32
			mspDesc.RasterDesc.CullMode = VkCullModeFlagBits::VK_CULL_MODE_BACK_BIT;
			if (!inoutPass.Compile(mspDesc))
			{
				return false;
			}

			return true;
		}

		void UpdateDirtyMaterials(Renderer::FrameAllocator& allocator)
		{
			Asset::MaterialInstanceBDA baseBDA = m_MaterialInstanceBuffer.GetDeviceAddress().value();
			for (auto& instance : m_DirtyInstances) // Mediocre at best, but good for now...
			{
				allocator.Allocate(instance->m_Data, m_MaterialInstanceBuffer, instance->GetBDA() - baseBDA);
			}
			m_DirtyInstances.clear();
		}

		RenderAPI::Core* m_diCore = nullptr;
		RenderAPI::Buffer m_MaterialInstanceBuffer;
		RenderAPI::Buffer m_MaterialInstanceCounterBuffer;
		aZero::FreelistAllocator m_Allocator;
		aZero::DS::IndexFreelist<Asset::MaterialRuntimeTypeID> m_MaterialTypeIndexList;
		std::unordered_map<std::string, MaterialAllocationInfo> m_Materials;
		//std::vector<Pipeline::RenderPass> m_Passes;
		std::unordered_set<const Asset::MaterialInstance*> m_DirtyInstances;
		std::unordered_map<std::string, MaterialInstanceAllocationInfo> m_MaterialInstances;
		uint32_t m_FrameIndex;
		Compiler::Compiler* m_diCompiler = nullptr;
		Compiler::CompilerSession m_Session;
		Compiler::ShaderModule m_TaskShader;
		Compiler::ShaderModule m_MeshShader;
		Compiler::ShaderModule m_FragmentShader;
	};
}
