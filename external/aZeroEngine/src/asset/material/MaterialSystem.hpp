#pragma once
#include <string>
#include <unordered_set>
#include <unordered_map>
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
			Pipeline::RenderPass RenderPass;
			Compiler::ShaderProgram ShaderProgram;
		};

		struct MaterialInstanceAllocationInfo
		{
			aZero::FreelistAllocator::Allocation Allocation;
			Asset::MaterialInstance Instance;
		};
	public:
		MaterialSystem() = default;
		MaterialSystem(RenderAPI::Core& core, Compiler::Compiler& diCompiler, Renderer::ResourceRecycler& recycler, uint64_t bufferSize, uint32_t framesInFlight)
			:m_diCore(&core), m_diCompiler(&diCompiler), m_Allocator(aZero::FreelistAllocator(bufferSize))
		{
			m_DeferredAllocationHandler.resize(framesInFlight);
			const RenderAPI::Buffer::Desc desc{
				.Size = bufferSize,
				.UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				.MemoryType = RenderAPI::EMemoryType::VRAM,
				.SeperateHeap = true,
				.OptResourceRecycler = recycler
			};
			m_MaterialInstanceBuffer.Init(core, desc);

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
			Asset::MaterialRuntimeTypeID newID = m_MaterialTypeIndexList.New();
			MaterialAllocationInfo info{
				.MaterialBase = Asset::MaterialType(definition, newID)
			};

			if (!this->CreatePipeline(info))
			{
				m_MaterialTypeIndexList.Recycle(newID);
				return std::nullopt;
			}

			if (auto iter = m_Materials.find(definition.m_Name); iter != m_Materials.end())
			{
				this->DestroyMaterial(iter->second.MaterialBase);
			}

			auto [it, inserted] = m_Materials.emplace(definition.m_Name, std::move(info));
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
			Asset::MaterialInstanceRuntimeID runtimeID = static_cast<Asset::MaterialInstanceRuntimeID>(m_MaterialInstanceBuffer.GetDeviceAddress().value() + allocation.Offset);

			MaterialInstanceAllocationInfo info{
				.Allocation = allocation,
				.Instance = Asset::MaterialInstance(material, name, runtimeID)
			};

			auto [it, inserted] = m_MaterialInstances.emplace(
				name,
				MaterialInstanceAllocationInfo{
					.Allocation = allocation,
					.Instance = Asset::MaterialInstance(material, name, runtimeID)
				});
			return it->second.Instance;
		}

		void DestroyMaterial(const Asset::MaterialType& material)
		{
			if (auto iter = m_Materials.find(material.GetDefinition().m_Name); iter != m_Materials.end())
			{
				if (iter->second.MaterialBase.GetRuntimeTypeID() == material.GetRuntimeTypeID())
				{
					std::erase_if(m_MaterialInstances, [&](auto& kv) {
						if (kv.second.Instance.m_diMaterial == &iter->second.MaterialBase)
						{
							m_DeferredAllocationHandler[m_FrameIndex].push_back(kv.second.Allocation);
							m_MaterialTypeIndexList.Recycle(kv.second.Instance.m_RuntimeID);
							kv.second.Instance.m_RuntimeID = Asset::g_InvalidRuntimeID;
							return true;
						}
						return false;
					});

					m_Materials.erase(iter);
				}
			}
		}

		void DestroyMaterialInstance(Asset::MaterialInstance& instance)
		{
			if (auto iter = m_MaterialInstances.find(instance.GetName()); iter != m_MaterialInstances.end())
			{
				if (iter->second.Instance.GetMaterial()->get().GetRuntimeTypeID() == instance.GetMaterial()->get().GetRuntimeTypeID())
				{
					m_DeferredAllocationHandler[m_FrameIndex].push_back(iter->second.Allocation);
					instance.m_RuntimeID = Asset::g_InvalidRuntimeID;
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

	private:
		bool CreatePipeline(MaterialAllocationInfo& allocationInfo)
		{
			Pipeline::RenderPass pass;
			Compiler::ShaderProgram program;

			Compiler::ShaderModule customBehavior = m_Session.LoadModule(std::string(ENGINE_SHADER_DIRECTORY) + allocationInfo.MaterialBase.GetDefinition().MaterialShaderPath.string()).value();

			Compiler::ShaderProgram::MeshletDesc msProgramDesc{
				.EntryPoints = {{"asMain"}, {"msMain"},{"psMain", {allocationInfo.MaterialBase.GetDefinition().MaterialShaderPath.filename().string()}}},
				.Modules = {&m_TaskShader, &m_MeshShader, &m_FragmentShader, &customBehavior /*todo Insert custom shadermodule*/}
			};

			if (!program.Build(msProgramDesc))
			{
				return false;
			}

			Pipeline::RenderPass::MeshShaderPassDesc mspDesc;
			mspDesc.Program = &allocationInfo.ShaderProgram;
			mspDesc.DepthDesc = Pipeline::RenderPass::Desc::DepthStencil();
			mspDesc.RenderTargetFormats = { VK_FORMAT_B8G8R8A8_SRGB };
			mspDesc.RasterDesc.CullMode = VkCullModeFlagBits::VK_CULL_MODE_NONE;
			if (!pass.Compile(mspDesc))
			{
				return false;
			}

			allocationInfo.ShaderProgram = std::move(program);
			allocationInfo.RenderPass = std::move(pass);

			return true;
		}

		void NextFrame(uint32_t frameIndex)
		{
			m_FrameIndex = frameIndex;
			if (frameIndex > m_DeferredAllocationHandler.size())
			{
				throw;
			}

			for (const auto& allocation : m_DeferredAllocationHandler[frameIndex])
			{
				m_Allocator.Free(allocation);
			}
		}

		void UpdateDirtyMaterials(Renderer::FrameAllocator& allocator)
		{
			for (auto& [name, materialInstance] : m_MaterialInstances) // Horrible, but good for now...
			{
				if (materialInstance.Instance.IsDirty())
				{
					allocator.Allocate(materialInstance.Instance.m_Data.data(), sizeof(std::byte) * materialInstance.Instance.m_Data.size(), m_MaterialInstanceBuffer, materialInstance.Instance.m_RuntimeID);
					materialInstance.Instance.SetNotDirty();
				}
			}
		}

		RenderAPI::Core* m_diCore = nullptr;
		RenderAPI::Buffer m_MaterialInstanceBuffer;
		aZero::FreelistAllocator m_Allocator;
		aZero::DS::IndexFreelist<Asset::MaterialRuntimeTypeID> m_MaterialTypeIndexList;
		std::unordered_map<std::string, MaterialAllocationInfo> m_Materials;
		std::unordered_map<std::string, MaterialInstanceAllocationInfo> m_MaterialInstances;
		std::vector<std::vector<aZero::FreelistAllocator::Allocation>> m_DeferredAllocationHandler;
		uint32_t m_FrameIndex;
		Compiler::Compiler* m_diCompiler = nullptr;
		Compiler::CompilerSession m_Session;
		Compiler::ShaderModule m_TaskShader;
		Compiler::ShaderModule m_MeshShader;
		Compiler::ShaderModule m_FragmentShader;
	};
}
