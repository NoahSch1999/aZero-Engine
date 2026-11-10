#pragma once
#include "asset/mesh/Mesh.hpp"

namespace aZero::Renderer { class Renderer; }

namespace aZero::System
{
	class MeshSystem
	{
		friend class aZero::Renderer::Renderer;

		struct GpuMeshletMesh
		{
			RenderAPI::Buffer VertexBuffer;
			RenderAPI::Buffer MeshletBuffer;
			RenderAPI::Buffer MeshletBoundsBuffer;
			Asset::Mesh Mesh;
		};
	public:
		MeshSystem() = default;
		MeshSystem(RenderAPI::Core& core, Renderer::ResourceRecycler& recycler)
			:m_diCore(&core), m_diRecycler(&recycler)
		{

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

			GpuMeshletMesh mesh;
			mesh.Mesh.m_Data = data;
			this->UploadMesh(mesh);
			auto [it, inserted] = m_MeshletMeshes.emplace(name, std::move(mesh));
			return it->second.Mesh;
		}

		void Destroy(const Asset::Mesh& mesh)
		{
			if (m_MeshletMeshes.contains(mesh.Name))
			{
				return;
			}

			m_MeshletMeshes.erase(mesh.Name);

			// todo Handle all references to the mesh
		}

		std::optional<std::reference_wrapper<Asset::Mesh>> GetMesh(const std::string& name)
		{
			if (auto iter = m_MeshletMeshes.find(name); iter != m_MeshletMeshes.end())
			{
				return iter->second.Mesh;
			}
			return std::nullopt;
		}

	private:

		void NextFrame(RenderAPI::CommandBuffer& diFrameCommandBuffer)
		{
			m_diFrameCommandBuffer = &diFrameCommandBuffer;
		}

		void UpdateDirtyMeshes()
		{
			for (auto& [name, gpuMesh] : m_MeshletMeshes) // Horrible, but good for now...
			{
				if (gpuMesh.Mesh.IsDirty())
				{
					this->UploadMesh(gpuMesh);
					gpuMesh.Mesh.SetNotDirty();

					// todo Handle all references to the mesh
				}
			}
		}

		void UploadMesh(GpuMeshletMesh& gpuMesh)
		{
			Asset::Mesh& mesh = gpuMesh.Mesh;
			// Whether or not to treat the mesh as a meshlet mesh -> task + mesh shader pipeline
			if (std::holds_alternative<Asset::MeshletMeshData>(mesh.m_Data))
			{
				const Asset::MeshletMeshData& meshData = std::get<Asset::MeshletMeshData>(mesh.m_Data);

				if (meshData.Meshlets.size() == 0 || meshData.Vertices.size() == 0 || meshData.MeshletBounds.size() == 0 || meshData.Submeshes.size() == 0)
				{
					return;
				}

				size_t totalSize = 0;

				size_t vertexDataSize = meshData.Vertices.size() * (size_t)sizeof(meshData.Vertices[0]);
				RenderAPI::Buffer::Desc vDesc{
					.Size = (uint64_t)vertexDataSize,
					.UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					.MemoryType = RenderAPI::EMemoryType::VRAM,
					.SeperateHeap = true,
					.OptResourceRecycler = *m_diRecycler
				};
				totalSize += vertexDataSize;

				size_t meshletDataSize = meshData.Meshlets.size() * (size_t)sizeof(meshData.Meshlets[0]);
				RenderAPI::Buffer::Desc mDesc{
					.Size = (uint64_t)meshletDataSize,
					.UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					.MemoryType = RenderAPI::EMemoryType::VRAM,
					.SeperateHeap = true,
					.OptResourceRecycler = *m_diRecycler
				};
				totalSize += meshletDataSize;

				size_t meshletBoundsDataSize = meshData.MeshletBounds.size() * (size_t)sizeof(meshData.MeshletBounds[0]);

				RenderAPI::Buffer::Desc mbDesc{
					.Size = (uint64_t)meshletBoundsDataSize,
					.UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
					.MemoryType = RenderAPI::EMemoryType::VRAM,
					.SeperateHeap = true,
					.OptResourceRecycler = *m_diRecycler
				};
				totalSize += meshletBoundsDataSize;

				gpuMesh.VertexBuffer = RenderAPI::Buffer(*m_diCore, vDesc);
				gpuMesh.MeshletBuffer = RenderAPI::Buffer(*m_diCore, mDesc);
				gpuMesh.MeshletBoundsBuffer = RenderAPI::Buffer(*m_diCore, mbDesc);

				RenderAPI::Buffer::Desc stagingBufferDesc{
					.Size = (uint64_t)totalSize,
					.UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					.MemoryType = RenderAPI::EMemoryType::SYSRAM,
					.SeperateHeap = true,
					.OptResourceRecycler = *m_diRecycler
				};
				RenderAPI::Buffer stagingBuffer(*m_diCore, stagingBufferDesc);
				stagingBuffer.Upload(meshData.Vertices, 0);
				stagingBuffer.Upload(meshData.Meshlets, vertexDataSize);
				stagingBuffer.Upload(meshData.MeshletBounds, vertexDataSize + meshletDataSize);

				m_diFrameCommandBuffer->Copy(stagingBuffer, 0, gpuMesh.VertexBuffer, 0, vertexDataSize);
				m_diFrameCommandBuffer->Copy(stagingBuffer, vertexDataSize, gpuMesh.MeshletBuffer, 0, meshletDataSize);
				m_diFrameCommandBuffer->Copy(stagingBuffer, vertexDataSize + meshletDataSize, gpuMesh.MeshletBoundsBuffer, 0, meshletBoundsDataSize);

				auto buildBarrier = [](const RenderAPI::Buffer& buffer) -> VkBufferMemoryBarrier2 {
					VkBufferMemoryBarrier2 stagingBufferBarrier = {
						.sType = VkStructureType::VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
						.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
						.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
						.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
						.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
						.buffer = buffer.Get(),
						.offset = 0,
						.size = buffer.GetDesc().Size
					};
					return stagingBufferBarrier;
					};
				std::vector<VkBufferMemoryBarrier2> barriers = {
					buildBarrier(gpuMesh.VertexBuffer),
					buildBarrier(gpuMesh.MeshletBuffer),
					buildBarrier(gpuMesh.MeshletBoundsBuffer)
				};

				m_diFrameCommandBuffer->Barrier({}, {}, { barriers });

				mesh.VertexBufferPtr = gpuMesh.VertexBuffer.GetDeviceAddress().value();
				mesh.MeshletBufferPtr = gpuMesh.MeshletBuffer.GetDeviceAddress().value();
				mesh.MeshletBoundsBufferPtr = gpuMesh.MeshletBoundsBuffer.GetDeviceAddress().value();

			}
			// Whether or not to treat the mesh as a classic indexed vertex buffer mesh -> input aseembler + vertex shader
			else if (std::holds_alternative<Asset::LegacyMeshData>(mesh.m_Data))
			{

			}
		}

		RenderAPI::CommandBuffer* m_diFrameCommandBuffer = nullptr;
		std::unordered_map<std::string, GpuMeshletMesh> m_MeshletMeshes;
		Renderer::ResourceRecycler* m_diRecycler = nullptr;
		RenderAPI::Core* m_diCore = nullptr;
	};
}