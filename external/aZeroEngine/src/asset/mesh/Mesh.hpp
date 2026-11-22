#pragma once
#include <variant>
#include "Primitives.hpp"
#include "asset/AssetData.hpp"
#include <numeric>

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

	struct PersistentMeshData
	{
		std::vector<Asset::MeshletSubMesh> Submeshes;
		std::vector<uint32_t> SubmeshMetadataSlots;
	};

	class Mesh : public Asset::AssetBase
	{
		friend class System::MeshSystem;
	public:
		Mesh() = default;
		Mesh(const std::variant<Asset::LegacyMeshData, Asset::MeshletMeshData>& data, System::MeshSystem& diMeshSystem);

		void SetDirty();

		// todo Handle legacy mesh
		const PersistentMeshData& GetPersistentData() const { return m_PersistentData; }
		bool IsValid() const { return !m_PersistentData.SubmeshMetadataSlots.empty() && m_PersistentData.SubmeshMetadataSlots.size() == m_PersistentData.Submeshes.size(); }
	private:
		std::variant<Asset::LegacyMeshData, Asset::MeshletMeshData> m_Data;

		// todo Store animation information such as per-vertex weights
		System::MeshSystem* m_diMeshSystem = nullptr;
		PersistentMeshData m_PersistentData;
	};
}