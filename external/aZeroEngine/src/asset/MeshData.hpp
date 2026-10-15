#pragma once
#include "RenderAssetBase.hpp"
#include "AssetData.hpp"

namespace aZero::Asset
{
	struct MeshData
	{
		std::string Name;
		std::string FilePath;
		MeshletMeshData m_Data;

		MeshData() = default;
	};
}