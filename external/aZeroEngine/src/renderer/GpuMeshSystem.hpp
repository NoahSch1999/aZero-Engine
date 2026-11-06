#pragma once
#include "asset/Mesh.hpp"

namespace aZero::Renderer
{
	class GpuMeshSystem
	{
	public:
		GpuMeshSystem() = default;

		~GpuMeshSystem()
		{
		}

		GpuMeshSystem(GpuMeshSystem&& other) noexcept
		{
			*this = std::move(other);
		}

		GpuMeshSystem& operator=(GpuMeshSystem&& other) noexcept
		{
			std::swap(m_MeshletMeshes, other.m_MeshletMeshes);
			return *this;
		}

		void Update(Asset::Mesh& mesh, RenderAPI::Core& core, ResourceRecycler& recycler, const RenderAPI::CommandBuffer& commandBuffer)
		{
			size_t totalSize = 0;

			size_t vertexDataSize = mesh.GetCachedData().Vertices.size() * (size_t)sizeof(mesh.GetCachedData().Vertices[0]);
			RenderAPI::Buffer::Desc vDesc {
				.Size = (uint64_t)vertexDataSize,
				.UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				.MemoryType = RenderAPI::EMemoryType::VRAM,
				.SeperateHeap = true,
				.OptResourceRecycler = recycler
			};
			totalSize += vertexDataSize;

			size_t meshletDataSize = mesh.GetCachedData().Meshlets.size() * (size_t)sizeof(mesh.GetCachedData().Meshlets[0]);
			RenderAPI::Buffer::Desc mDesc {
				.Size = (uint64_t)meshletDataSize,
				.UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				.MemoryType = RenderAPI::EMemoryType::VRAM,
				.SeperateHeap = true,
				.OptResourceRecycler = recycler
			};
			totalSize += meshletDataSize;

			size_t meshletBoundsDataSize = mesh.GetCachedData().MeshletBounds.size() * (size_t)sizeof(mesh.GetCachedData().MeshletBounds[0]);
			
			RenderAPI::Buffer::Desc mbDesc {
				.Size = (uint64_t)meshletBoundsDataSize,
				.UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				.MemoryType = RenderAPI::EMemoryType::VRAM,
				.SeperateHeap = true,
				.OptResourceRecycler = recycler
			};
			totalSize += meshletBoundsDataSize;
			
			GpuMeshletMesh gpuMeshletMesh;
			gpuMeshletMesh.VertexBuffer = RenderAPI::Buffer(core, vDesc);
			gpuMeshletMesh.MeshletBuffer = RenderAPI::Buffer(core, mDesc);
			gpuMeshletMesh.MeshletBoundsBuffer = RenderAPI::Buffer(core, mbDesc);

			RenderAPI::Buffer::Desc stagingBufferDesc{
				.Size = (uint64_t)totalSize,
				.UsageBits = VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				.MemoryType = RenderAPI::EMemoryType::SYSRAM,
				.SeperateHeap = true,
				.OptResourceRecycler = recycler
			};
			RenderAPI::Buffer stagingBuffer(core, stagingBufferDesc);
			stagingBuffer.Upload(mesh.GetCachedData().Vertices, 0);
			stagingBuffer.Upload(mesh.GetCachedData().Meshlets, vertexDataSize);
			stagingBuffer.Upload(mesh.GetCachedData().MeshletBounds, vertexDataSize + meshletDataSize);

			commandBuffer.Copy(stagingBuffer, 0, gpuMeshletMesh.VertexBuffer, 0, vertexDataSize);
			commandBuffer.Copy(stagingBuffer, vertexDataSize, gpuMeshletMesh.MeshletBuffer, 0, meshletDataSize);
			commandBuffer.Copy(stagingBuffer, vertexDataSize + meshletDataSize, gpuMeshletMesh.MeshletBoundsBuffer, 0, meshletDataSize);

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
				buildBarrier(gpuMeshletMesh.VertexBuffer),
				buildBarrier(gpuMeshletMesh.MeshletBuffer),
				buildBarrier(gpuMeshletMesh.MeshletBoundsBuffer)
			};

			commandBuffer.Barrier({}, {}, { barriers });

			mesh.m_RenderRef.VertexPtr = gpuMeshletMesh.VertexBuffer.GetDeviceAddress().value();
			mesh.m_RenderRef.MeshletPtr = gpuMeshletMesh.MeshletBuffer.GetDeviceAddress().value();
			mesh.m_RenderRef.MeshletBoundPtr = gpuMeshletMesh.MeshletBoundsBuffer.GetDeviceAddress().value();

			m_MeshletMeshes[mesh.m_RenderRef.VertexPtr] = std::move(gpuMeshletMesh);
		}

		void Destroy(Asset::Mesh& mesh)
		{
			if (!mesh.GetRenderRef().IsValid())
			{
				return;
			}
			m_MeshletMeshes.erase(mesh.m_RenderRef.VertexPtr);
			mesh.m_RenderRef = Asset::MeshRenderRef();
		}

	private:
		struct GpuMeshletMesh
		{
			RenderAPI::Buffer VertexBuffer;
			RenderAPI::Buffer MeshletBuffer;
			RenderAPI::Buffer MeshletBoundsBuffer;
		};
		std::unordered_map<VkDeviceAddress, GpuMeshletMesh> m_MeshletMeshes;
	};
}