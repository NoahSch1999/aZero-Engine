#pragma once
#include <variant>
#include "Primitives.hpp"
#include "asset/AssetData.hpp"

namespace aZero::System { class MeshSystem; }

namespace aZero::Asset
{
	struct LegacyMeshData : public AssetBase
	{
		std::vector<aZero::Asset::Vertex> Vertices;
		std::vector<aZero::Asset::VertexIndex> Indices;
		std::vector<aZero::Asset::LegacySubMesh> Submeshes;
	};

	struct MeshletMeshData : public AssetBase
	{
		std::vector<aZero::Asset::Meshlet> Meshlets;
		std::vector<BoundingVolume::BoundingSphere> MeshletBounds;
		std::vector<aZero::Asset::Vertex> Vertices;
		std::vector<aZero::Asset::MeshletSubMesh> Submeshes;

		bool HasValidData() const
		{
			return !Meshlets.empty() && !MeshletBounds.empty() && !Vertices.empty() && !Submeshes.empty();
		}
	};

	using MeshBufferPtr = VkDeviceAddress;
	constexpr MeshBufferPtr g_InvalidMeshBufferPtr = std::numeric_limits<MeshBufferPtr>::max();
	class Mesh : public Asset::AssetBase
	{
		friend class System::MeshSystem;
	public:
		Mesh() = default;
		Mesh(const std::variant<Asset::LegacyMeshData, Asset::MeshletMeshData>& data)
			:m_Data(data)
		{
			m_MeshletCount = std::get<Asset::MeshletMeshData>(m_Data).Meshlets.size();
			m_Submeshes = std::get<Asset::MeshletMeshData>(m_Data).Submeshes;
		}
		void SetData(const std::variant<Asset::LegacyMeshData, Asset::MeshletMeshData>& data) { 
			m_Data = data; m_IsDirty = true; 
			m_MeshletCount = std::get<Asset::MeshletMeshData>(m_Data).Meshlets.size();
			m_Submeshes = std::get<Asset::MeshletMeshData>(m_Data).Submeshes;
		}
		void SetNotDirty() { m_IsDirty = false; }
		bool IsDirty() const { return m_IsDirty; }

		// todo Handle legacy mesh
		MeshBufferPtr GetVertexGpuPtr() const { return VertexBufferPtr; }
		MeshBufferPtr GetMeshletGpuPtr() const { return MeshletBufferPtr; }
		MeshBufferPtr GetMeshletBoundsGpuPtr() const { return MeshletBoundsBufferPtr; }
		uint32_t GetMeshletCount() const { return m_MeshletCount; }
		const std::vector<Asset::MeshletSubMesh>& GetSubmeshes() const { return m_Submeshes; }
		bool IsValid() const { return VertexBufferPtr != g_InvalidMeshBufferPtr; }
	private:
		std::variant<Asset::LegacyMeshData, Asset::MeshletMeshData> m_Data;

		// todo Store animation information such as per-vertex weights
		bool m_IsDirty = true;
		uint32_t m_MeshletCount;
		std::vector<Asset::MeshletSubMesh> m_Submeshes;
		MeshBufferPtr VertexBufferPtr = g_InvalidMeshBufferPtr, MeshletBufferPtr = g_InvalidMeshBufferPtr, MeshletBoundsBufferPtr = g_InvalidMeshBufferPtr;
	};
}