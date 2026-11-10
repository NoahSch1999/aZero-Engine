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
		}
		void SetData(const std::variant<Asset::LegacyMeshData, Asset::MeshletMeshData>& data) { m_Data = data; m_IsDirty = true; }
		void SetNotDirty() { m_IsDirty = false; }
		bool IsDirty() const { return m_IsDirty; }

	private:
		std::variant<Asset::LegacyMeshData, Asset::MeshletMeshData> m_Data;

		// todo Store animation information such as per-vertex weights
		bool m_IsDirty = true;
		MeshBufferPtr VertexBufferPtr = g_InvalidMeshBufferPtr, MeshletBufferPtr = g_InvalidMeshBufferPtr, MeshletBoundsBufferPtr = g_InvalidMeshBufferPtr;
	};
}