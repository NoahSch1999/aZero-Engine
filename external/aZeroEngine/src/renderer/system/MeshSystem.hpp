#pragma once
#include "asset/mesh/Mesh.hpp"
#include "renderer/FrameAllocator.hpp"
#include "misc/IndexFreelist.hpp"
#include "renderer/mesh_draw_pipeline/Mesh_Draw_Structs.hpp"
#include <unordered_set>

namespace aZero::Renderer { class Renderer; }

namespace aZero::System
{
	class MeshSystem
	{
		friend class aZero::Renderer::Renderer;

		struct GpuMeshletMesh
		{
			RenderAPI::Buffer Buffer;
			Asset::Mesh Mesh;
		};
	public:
		static inline const char* s_DefaultMeshStr = "DefaultMesh";
		MeshSystem() = default;
		MeshSystem(RenderAPI::Core& core, Renderer::ResourceRecycler& recycler)
			:m_diCore(&core), m_diRecycler(&recycler)
		{
			RenderAPI::Buffer::Desc desc{
					.Size = (uint64_t)10000 * sizeof(GPUStruct::SubmeshMetadata), // todo Make setting/expandable
					.UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					.MemoryType = RenderAPI::EMemoryType::VRAM,
					.SeperateHeap = true,
					.Name = "MeshSystem::m_SlotBuffer::",
					.OptResourceRecycler = *m_diRecycler
			};

			m_SlotBuffer = RenderAPI::Buffer(*m_diCore, desc);
		}

		~MeshSystem()
		{
		}

		MeshSystem(MeshSystem&& other) noexcept
		{
			*this = std::move(other);
		}

		MeshSystem& operator=(MeshSystem&& other) noexcept
		{
			std::swap(m_diFrameCommandBuffer, other.m_diFrameCommandBuffer);
			std::swap(m_MeshletMeshes, other.m_MeshletMeshes);
			std::swap(m_diRecycler, other.m_diRecycler);
			std::swap(m_diCore, other.m_diCore);
			return *this;
		}

		std::optional<std::reference_wrapper<Asset::Mesh>> CreateMesh(const std::string& name, const std::variant<Asset::LegacyMeshData, Asset::MeshletMeshData>& data)
		{
			if (m_MeshletMeshes.contains(name) || !std::get<Asset::MeshletMeshData>(data).HasValidData()) // todo Handle LegacyMeshData
			{
				return std::nullopt;
			}

			// todo Extract total size and num submeshes and then spin up processing/loading on a thread
			const Asset::MeshletMeshData& meshData = std::get<Asset::MeshletMeshData>(data);
			size_t vertexDataSize = meshData.Vertices.size() * (size_t)sizeof(meshData.Vertices[0]);
			size_t meshletDataSize = meshData.Meshlets.size() * (size_t)sizeof(meshData.Meshlets[0]);
			size_t meshletBoundsDataSize = meshData.MeshletBounds.size() * (size_t)sizeof(meshData.MeshletBounds[0]);

			GpuMeshletMesh mesh;
			mesh.Mesh.Name = name;
			mesh.Mesh.m_diMeshSystem = this;
			mesh.Mesh.m_Data = data;
			mesh.Mesh.m_PersistentData.Submeshes = meshData.Submeshes;

			// todo After this and the size of the data is known, then we can queue everything async
			mesh.Mesh.m_PersistentData.SubmeshMetadataSlots = this->AllocateMetadataSlots(meshData.Submeshes.size());

			auto [it, inserted] = m_MeshletMeshes.emplace(name, std::move(mesh));

			// todo Async
			auto [dstBuffer, stagingBuffer] = this->AllocateGpuMemory(vertexDataSize + meshletDataSize + meshletBoundsDataSize, name);
			//

			// todo Execute when AllocateGpuMemory is complete
			this->PopulateGpuMemory(dstBuffer, stagingBuffer, it->second.Mesh);
			//

			it->second.Buffer = std::move(dstBuffer);

			return it->second.Mesh;
		}

		void Destroy(const Asset::Mesh& mesh)
		{
			if (m_MeshletMeshes.contains(mesh.Name))
			{
				return;
			}

			this->DeallocateMetadataSlots(mesh.m_PersistentData.SubmeshMetadataSlots);
			m_MeshletMeshes.erase(mesh.Name);
		}

		std::optional<std::reference_wrapper<Asset::Mesh>> GetMesh(const std::string& name)
		{
			if (auto iter = m_MeshletMeshes.find(name); iter != m_MeshletMeshes.end())
			{
				return iter->second.Mesh;
			}
			return std::nullopt;
		}

		void SetMeshDirty(const Asset::Mesh& mesh)
		{
			m_DirtyMeshes.emplace(&mesh);
		}

		// todo Decide if this should be re-implemented
		void UpdateDirtyMeshes()
		{
			if (!m_DirtyMeshes.empty())
			{
				// todo Record upload metadata + transition buffer
			}
			for (const auto& mesh : m_DirtyMeshes)
			{
				if (auto iter = m_MeshletMeshes.find(mesh->Name); iter != m_MeshletMeshes.end())
				{
					//this->UploadMesh(iter->second);

					// todo Redirect mesh components that reference the mesh
				}
			}
			// todo transition buffer
			m_DirtyMeshes.clear();
		}
	private:

		void NextFrame(RenderAPI::CommandBuffer& diFrameCommandBuffer, Renderer::FrameAllocator& diAllocator)
		{
			m_diFrameCommandBuffer = &diFrameCommandBuffer;
			m_diAllocator = &diAllocator;
		}

		// Returns slot indices for the submesh's metadata and allocates dummy-data on the gpu.
		// Ran on the main thread before spinning off async work for AllocateGpuMemory
		std::vector<uint32_t> AllocateMetadataSlots(uint32_t numSubmeshes)
		{
			std::vector<uint32_t> indices(numSubmeshes);
			for (uint32_t i = 0; i < numSubmeshes; i++)
			{
				indices[i] = m_SlotAllocator.New();
				GPUStruct::SubmeshMetadata dummy{ m_EmptySubmeshMetadata };
				m_diAllocator->Allocate(&dummy, sizeof(dummy), m_SlotBuffer, sizeof(dummy) * indices[i]);
			}
			return indices;
		}

		void DeallocateMetadataSlots(const std::vector<uint32_t>& slots)
		{
			for (const auto& submeshIndex : slots)
			{
				m_SlotAllocator.Recycle(submeshIndex);
			}
		}

		// Seperate thread
		std::pair<RenderAPI::Buffer, RenderAPI::Buffer> AllocateGpuMemory(uint64_t dataSize, const std::string& name = "")
		{
			RenderAPI::Buffer::Desc desc{
					.Size = dataSize,
					.UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					.MemoryType = RenderAPI::EMemoryType::VRAM,
					.SeperateHeap = true,
					.Name = name,
					.OptResourceRecycler = *m_diRecycler
			};

			RenderAPI::Buffer dstBuffer(*m_diCore, desc);

			desc.UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			RenderAPI::Buffer stagingBuffer(*m_diCore, desc);
			return std::make_pair(std::move(dstBuffer), std::move(stagingBuffer));
		}

		// Main thread
		void PopulateGpuMemory(RenderAPI::Buffer& dstBuffer, RenderAPI::Buffer& stagingBuffer, const Asset::Mesh& mesh)
		{
			const Asset::MeshletMeshData& meshData = std::get<Asset::MeshletMeshData>(mesh.m_Data);
			size_t vertexDataSize = meshData.Vertices.size() * (size_t)sizeof(meshData.Vertices[0]);
			size_t meshletDataSize = meshData.Meshlets.size() * (size_t)sizeof(meshData.Meshlets[0]);
			size_t meshletBoundsDataSize = meshData.MeshletBounds.size() * (size_t)sizeof(meshData.MeshletBounds[0]);

			stagingBuffer.Upload(meshData.Vertices, 0);
			stagingBuffer.Upload(meshData.Meshlets, vertexDataSize);
			stagingBuffer.Upload(meshData.MeshletBounds, vertexDataSize + meshletDataSize);

			for (uint32_t i = 0; i < mesh.m_PersistentData.SubmeshMetadataSlots.size(); i++)
			{
				GPUStruct::SubmeshMetadata submeshMetadata{
					.Bounds = mesh.m_PersistentData.Submeshes[i].Bounds,
					.VertexBufferPtr = dstBuffer.GetDeviceAddress().value(),
					.MeshletBufferPtr = dstBuffer.GetDeviceAddress().value() + vertexDataSize + sizeof(Asset::Meshlet) * mesh.m_PersistentData.Submeshes[i].MeshletOffset,
					.MeshletBoundsBufferPtr = dstBuffer.GetDeviceAddress().value() + vertexDataSize + meshletDataSize + sizeof(BoundingVolume::BoundingSphere) * mesh.m_PersistentData.Submeshes[i].MeshletOffset,
					.MeshletCount = mesh.m_PersistentData.Submeshes[i].MeshletCount
				};
				m_diAllocator->Allocate(&submeshMetadata, sizeof(submeshMetadata), m_SlotBuffer, sizeof(submeshMetadata) * mesh.m_PersistentData.SubmeshMetadataSlots[i]);
			}

			{
				aZero::RenderAPI::Buffer::TransitionDesc trDesc{
							.DstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
							.DstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
							.Offset = 0,
							.Size = dstBuffer.GetDesc().Size
				};
				dstBuffer.Transition(trDesc);
				m_diFrameCommandBuffer->Barrier({}, {}, { dstBuffer.GetTransition() });
			}
			m_diFrameCommandBuffer->Copy(stagingBuffer, 0, dstBuffer, 0, dstBuffer.GetDesc().Size);
			{
				aZero::RenderAPI::Buffer::TransitionDesc trDesc{
							.DstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
							.DstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
							.Offset = 0,
							.Size = dstBuffer.GetDesc().Size
				};
				dstBuffer.Transition(trDesc);
				m_diFrameCommandBuffer->Barrier({}, {}, { dstBuffer.GetTransition() });
			}
		}

		std::unordered_set<const Asset::Mesh*> m_DirtyMeshes;
		RenderAPI::CommandBuffer* m_diFrameCommandBuffer = nullptr;
		Renderer::FrameAllocator* m_diAllocator = nullptr;
		std::unordered_map<std::string, GpuMeshletMesh> m_MeshletMeshes;

		aZero::DS::IndexFreelist<uint32_t> m_SlotAllocator;
		aZero::RenderAPI::Buffer m_SlotBuffer;

		GPUStruct::SubmeshMetadata m_EmptySubmeshMetadata{ .Bounds = {.Origin = {}, .Radius = 0.f}, .VertexBufferPtr = 0, .MeshletBufferPtr = 0, .MeshletBoundsBufferPtr = 0, .MeshletCount = 0};

		Renderer::ResourceRecycler* m_diRecycler = nullptr;
		RenderAPI::Core* m_diCore = nullptr;
	};
}