#pragma once
#include <vector>
#include <ktx.h>
#include "Primitives.hpp"
#include "misc/UUID.hpp"
#include "render_api/Image.hpp"

namespace aZero::Asset
{
	struct AssetBase
	{
		std::string Name;
		std::array<char, aZero::Helper::UUID::GetSizeAsString()> UUID;
	};

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
	};

	struct ImageData : public AssetBase
	{
		struct Mip
		{
			std::uint32_t Offset;
			std::uint32_t RowPitch;
			std::uint32_t SlicePitch;
		};

		uint32_t BaseWidth;
		uint32_t BaseHeight;
		uint32_t FaceCount;
		uint32_t DepthOrArrayCount;
		std::vector<std::byte> Data;
		std::vector<Mip> MipLevels;
		VkFormat Format;
		VkImageType Type;
	};
}