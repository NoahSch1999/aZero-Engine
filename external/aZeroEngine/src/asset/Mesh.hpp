#pragma once
#include "AssetData.hpp"

namespace aZero::Asset
{
	struct MeshRenderRef
	{
		VkDeviceAddress VertexPtr = 0, MeshletPtr = 0, MeshletBoundPtr = 0;
		bool IsValid() const { return VertexPtr != 0; }
	};

	class Mesh : public RenderAssetBase<MeshRenderRef, MeshletMeshData>
	{
	public:
		Mesh() = default;
		Mesh(MeshletMeshData&& data)
			:RenderAssetBase(std::move(data))
		{
			const auto& cachedData = this->GetCachedData();
			for (const auto& mesh : cachedData.Submeshes)
			{
				m_Submeshes[m_NumSubmeshes] = mesh;
				m_NumSubmeshes++;
			}
		}

		Mesh(const MeshletMeshData& data)
			:RenderAssetBase(data)
		{
			const auto& cachedData = this->GetCachedData();
			for (const auto& mesh : cachedData.Submeshes)
			{
				m_Submeshes[m_NumSubmeshes] = mesh;
				m_NumSubmeshes++;
			}
		}

		void ClearCachedData() { 
			m_CachedData = {};
		}

		std::pair<uint32_t, std::array<MeshletSubMesh, 10>> GetSubmeshes() const { return std::make_pair(m_NumSubmeshes, m_Submeshes); } // todo Return reference to array

	private:
		std::array<MeshletSubMesh, 10> m_Submeshes;
		uint32_t m_NumSubmeshes = 0;
	};
}