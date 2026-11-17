#pragma once
#include <filesystem>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include "asset/AssetData.hpp"
#include "ImageLoading.hpp"
#include "asset/material/PBR_Material.hpp"
#include "misc/Threadpool.hpp"

namespace aZero::Asset
{
	inline aZero::Asset::LegacyMeshData ExtractGltfLegacyMesh(const fastgltf::Mesh& mesh, fastgltf::Asset* asset)
	{
		const uint32_t numPrimitives = std::clamp((uint32_t)mesh.primitives.size(), 0u, aZero::Asset::g_MaxNumSubmeshes); // Currently only supports atmost 10 submeshes
		aZero::Asset::LegacyMeshData meshData;
		meshData.Name = mesh.name.c_str();
		meshData.Submeshes.resize(numPrimitives);

		uint32_t vertexOffset = 0;
		for (int i = 0; i < numPrimitives; i++)
		{
			const auto& primitive = mesh.primitives[i];
			size_t baseColorTexcoordIndex = 0;

			if (primitive.materialIndex.has_value())
			{
				const fastgltf::Material& material = asset->materials[primitive.materialIndex.value()];
				if (material.pbrData.baseColorTexture.has_value())
				{
					if (material.pbrData.baseColorTexture->transform && material.pbrData.baseColorTexture->transform->texCoordIndex.has_value()) {
						baseColorTexcoordIndex = material.pbrData.baseColorTexture->transform->texCoordIndex.value();
					}
					else {
						baseColorTexcoordIndex = material.pbrData.baseColorTexture->texCoordIndex;
					}
				}
			}

			auto& positionAccessor = asset->accessors[primitive.findAttribute("POSITION")->accessorIndex];
			/*if (!positionAccessor.bufferViewIndex.has_value())
				continue;*/

			std::vector<Asset::Vertex> vertexData(positionAccessor.count);
			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(*asset, positionAccessor, [&vertexData](fastgltf::math::fvec3 pos, std::size_t idx) {
				auto position = fastgltf::math::fvec3(pos.x(), pos.y(), pos.z());
				vertexData[idx].Position = { position.x(), position.y(), position.z() };
				});

			auto& normalAccessor = asset->accessors[primitive.findAttribute("NORMAL")->accessorIndex];
			/*if (!normalAccessor.bufferViewIndex.has_value())
				continue;*/

			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(*asset, normalAccessor, [&vertexData](fastgltf::math::fvec3 n, std::size_t idx) {
				vertexData[idx].Normal = Asset::PackNormal(Asset::EncodeNormalOctahedral({ n.x(), n.y(), n.z() }));
				});

			const std::string texcoordAttribute = std::string("TEXCOORD_") + std::to_string(baseColorTexcoordIndex);
			if (const auto* texcoord = primitive.findAttribute(texcoordAttribute); texcoord != primitive.attributes.end()) {
				// Tex coord
				auto& texCoordAccessor = asset->accessors[texcoord->accessorIndex];
				/*if (!texCoordAccessor.bufferViewIndex.has_value())
					continue;*/

				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(*asset, texCoordAccessor, [&](fastgltf::math::fvec2 uv, std::size_t idx) {
					vertexData[idx].UV = Asset::PackUV({ uv.x(), uv.y() });
					});
			}

			auto& indexAccessor = asset->accessors[primitive.indicesAccessor.value()];
			/*if (!indexAccessor.bufferViewIndex.has_value())
				return false;*/


			std::vector<uint32_t> indices(indexAccessor.count);
			if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedByte || indexAccessor.componentType == fastgltf::ComponentType::UnsignedShort)
			{
				// Only 32bit indices are supported. With mesh shaders its not gonna matter anyways since the indices will be remapped to 8bits.
				std::vector<uint16_t> tempIndices(indexAccessor.count);
				fastgltf::copyFromAccessor<uint16_t>(*asset, indexAccessor, tempIndices.data());
				for (int j = 0; j < tempIndices.size(); j++) {
					indices[j] = tempIndices[j];
				}
			}
			else
			{
				fastgltf::copyFromAccessor<uint32_t>(*asset, indexAccessor, indices.data());
			}

			Asset::LegacySubMesh newSubmesh;
			newSubmesh.Bounds = Asset::ComputeBoundingSphere(vertexData);
			newSubmesh.IndexOffset = meshData.Indices.size();
			newSubmesh.IndexCount = indices.size();
			vertexOffset += vertexData.size();
			meshData.Submeshes[i] = newSubmesh;

			meshData.Vertices.insert(meshData.Vertices.end(), vertexData.begin(), vertexData.end());
			meshData.Indices.insert(meshData.Indices.end(), indices.begin(), indices.end());
		}

		return meshData;
	}

	inline aZero::Asset::MeshletMeshData ExtractGltfMeshletMesh(const fastgltf::Mesh& mesh, fastgltf::Asset* asset)
	{
		const uint32_t numPrimitives = std::clamp((uint32_t)mesh.primitives.size(), 0u, aZero::Asset::g_MaxNumSubmeshes); // Currently only supports atmost 10 submeshes
		aZero::Asset::MeshletMeshData meshData;
		meshData.Name = mesh.name.c_str();
		meshData.Submeshes.resize(numPrimitives);

		uint32_t vertexOffset = 0;
		for (int i = 0; i < numPrimitives; i++)
		{
			const auto& primitive = mesh.primitives[i];
			size_t baseColorTexcoordIndex = 0;

			if (primitive.materialIndex.has_value())
			{
				const fastgltf::Material& material = asset->materials[primitive.materialIndex.value()];
				if (material.pbrData.baseColorTexture.has_value())
				{
					if (material.pbrData.baseColorTexture->transform && material.pbrData.baseColorTexture->transform->texCoordIndex.has_value()) {
						baseColorTexcoordIndex = material.pbrData.baseColorTexture->transform->texCoordIndex.value();
					}
					else {
						baseColorTexcoordIndex = material.pbrData.baseColorTexture->texCoordIndex;
					}
				}
			}

			auto& positionAccessor = asset->accessors[primitive.findAttribute("POSITION")->accessorIndex];
			/*if (!positionAccessor.bufferViewIndex.has_value())
				continue;*/

			std::vector<Asset::Vertex> vertexData(positionAccessor.count);
			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(*asset, positionAccessor, [&vertexData](fastgltf::math::fvec3 pos, std::size_t idx) {
				auto position = fastgltf::math::fvec3(pos.x(), pos.y(), pos.z());
				vertexData[idx].Position = { position.x(), position.y(), position.z() };
				});

			auto& normalAccessor = asset->accessors[primitive.findAttribute("NORMAL")->accessorIndex];
			/*if (!normalAccessor.bufferViewIndex.has_value())
				continue;*/

			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(*asset, normalAccessor, [&vertexData](fastgltf::math::fvec3 n, std::size_t idx) {
				vertexData[idx].Normal = Asset::PackNormal(Asset::EncodeNormalOctahedral({ n.x(), n.y(), n.z() }));
				});

			const std::string texcoordAttribute = std::string("TEXCOORD_") + std::to_string(baseColorTexcoordIndex);
			if (const auto* texcoord = primitive.findAttribute(texcoordAttribute); texcoord != primitive.attributes.end()) {
				// Tex coord
				auto& texCoordAccessor = asset->accessors[texcoord->accessorIndex];
				/*if (!texCoordAccessor.bufferViewIndex.has_value())
					continue;*/

				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(*asset, texCoordAccessor, [&](fastgltf::math::fvec2 uv, std::size_t idx) {
					vertexData[idx].UV = Asset::PackUV({ uv.x(), uv.y() });
					});
			}

			auto& indexAccessor = asset->accessors[primitive.indicesAccessor.value()];
			/*if (!indexAccessor.bufferViewIndex.has_value())
				return false;*/


			std::vector<uint32_t> indices(indexAccessor.count);
			if (indexAccessor.componentType == fastgltf::ComponentType::UnsignedByte || indexAccessor.componentType == fastgltf::ComponentType::UnsignedShort)
			{
				// Only 32bit indices are supported. With mesh shaders its not gonna matter anyways since the indices will be remapped to 8bits.
				std::vector<uint16_t> tempIndices(indexAccessor.count);
				fastgltf::copyFromAccessor<uint16_t>(*asset, indexAccessor, tempIndices.data());
				for (int j = 0; j < tempIndices.size(); j++) {
					indices[j] = tempIndices[j];
				}
			}
			else
			{
				fastgltf::copyFromAccessor<uint32_t>(*asset, indexAccessor, indices.data());
			}

			std::vector<Asset::Meshlet> meshlets;
			std::vector<BoundingVolume::BoundingSphere> meshletBounds;
			Asset::Meshletize(vertexData, indices, meshlets, meshletBounds, vertexOffset);

			Asset::MeshletSubMesh newSubmesh;
			newSubmesh.Bounds = Asset::ComputeBoundingSphere(vertexData);
			newSubmesh.MeshletOffset = meshData.Meshlets.size();
			newSubmesh.MeshletCount = meshlets.size();
			vertexOffset += vertexData.size();
			meshData.Submeshes[i] = newSubmesh;

			meshData.Vertices.insert(meshData.Vertices.end(), vertexData.begin(), vertexData.end());
			meshData.Meshlets.insert(meshData.Meshlets.end(), meshlets.begin(), meshlets.end());
			meshData.MeshletBounds.insert(meshData.MeshletBounds.end(), meshletBounds.begin(), meshletBounds.end());
		}

		return meshData;
	}

	inline std::optional<aZero::Asset::ImageData> ExtractGltfImage(const fastgltf::Image& image, fastgltf::Asset* asset, const std::filesystem::path& path)
	{
		std::optional<aZero::Asset::ImageData> imageData;

		std::visit(fastgltf::visitor{
				[](const auto& arg) {},
				[&imageData, &path](const fastgltf::sources::URI& filePath) {
				// todo Figure out how to handle formatting
				std::filesystem::path relative(filePath.uri.path());
				imageData = aZero::Asset::LoadImageData(path.parent_path() / relative, true);
			},
			[&imageData, &image](const fastgltf::sources::Array& vector) {
				imageData = aZero::Asset::LoadImageDataFromMemory(image.name.c_str(), vector.bytes.data(), vector.bytes.size(), 0);
			},
			[asset, &imageData, &image](const fastgltf::sources::BufferView& view) {
				auto& bufferView = asset->bufferViews[view.bufferViewIndex];
				auto& buffer = asset->buffers[bufferView.bufferIndex];
				std::visit(fastgltf::visitor{
					[](const auto& arg) {},
					[&](const fastgltf::sources::Array& vector) {
						imageData = aZero::Asset::LoadImageDataFromMemory(image.name.c_str(), vector.bytes.data(), bufferView.byteLength, bufferView.byteOffset);
					}
				}, buffer.data);
			}
			}, image.data);

		return imageData;
	}

	// Returns a material with the image index instead of descriptor index
	inline Asset::PBRMaterialData ExtractPBRMaterial(const fastgltf::Material& material, fastgltf::Asset* asset)
	{
		Asset::PBRMaterialData pbrData;

		if (material.pbrData.baseColorTexture.has_value()) {
			auto imageIndex = asset->textures[material.pbrData.baseColorTexture.value().textureIndex].imageIndex.has_value() ?
				asset->textures[material.pbrData.baseColorTexture.value().textureIndex].imageIndex.value() :
				asset->textures[material.pbrData.baseColorTexture.value().textureIndex].basisuImageIndex.has_value() ? asset->textures[material.pbrData.baseColorTexture.value().textureIndex].basisuImageIndex.value() :
				std::numeric_limits<uint32_t>::max();

			pbrData.AlbedoTexture = imageIndex;
		}

		if (material.normalTexture.has_value()) {
			auto imageIndex = asset->textures[material.normalTexture.value().textureIndex].imageIndex.has_value() ?
				asset->textures[material.normalTexture.value().textureIndex].imageIndex.value() :
				asset->textures[material.normalTexture.value().textureIndex].basisuImageIndex.has_value() ? asset->textures[material.normalTexture.value().textureIndex].basisuImageIndex.value() :
				std::numeric_limits<uint32_t>::max();

			pbrData.NormalMap = imageIndex;
		}

		if (material.pbrData.metallicRoughnessTexture.has_value()) {
			// todo Impl
		}

		pbrData.RoughnessFactor = material.pbrData.roughnessFactor;
		pbrData.MetallicFactor = material.pbrData.metallicFactor;

		return pbrData;
	}

	class LoadedGltf
	{
	public:
		fastgltf::Asset LoadedAsset;

		std::vector<std::optional<Asset::ImageData>> ImageData;
		std::vector<Asset::MeshletMeshData> MeshData;
		std::vector<std::pair<std::string, Asset::PBRMaterialData>> Materials;

		LoadedGltf() = default;
		LoadedGltf(const std::filesystem::path& path)
		{
			this->Load(path);
		}

		bool Load(const std::filesystem::path& path)
		{
			static constexpr auto supportedExtensions =
				fastgltf::Extensions::KHR_mesh_quantization |
				fastgltf::Extensions::KHR_texture_transform |
				fastgltf::Extensions::KHR_materials_variants |
				fastgltf::Extensions::KHR_lights_punctual |
				fastgltf::Extensions::KHR_texture_basisu
				;

			fastgltf::Parser parser(supportedExtensions);

			constexpr auto gltfOptions =
				fastgltf::Options::DontRequireValidAssetMember |
				fastgltf::Options::AllowDouble |
				fastgltf::Options::LoadExternalBuffers |
				//fastgltf::Options::LoadExternalImages |
				fastgltf::Options::GenerateMeshIndices |
				fastgltf::Options::DecomposeNodeMatrices
				;

			auto gltfFile = fastgltf::MappedGltfFile::FromPath(path);
			if (!bool(gltfFile)) {
				std::cerr << "Failed to open glTF file: " << fastgltf::getErrorMessage(gltfFile.error()) << '\n';
				return false;
			}

			auto loadedAsset = parser.loadGltf(gltfFile.get(), path.parent_path(), gltfOptions);
			fastgltf::Asset* asset = loadedAsset.get_if();
			if (!asset) {
				std::cerr << "Failed to load glTF file: " << fastgltf::getErrorMessage(loadedAsset.error()) << '\n';
				return false;
			}

			LoadedAsset = std::move(*asset);

			aZero::ThreadPool tp(std::thread::hardware_concurrency());
			std::vector<std::future<void>> tpHandles;

			std::chrono::high_resolution_clock clock;

			auto start = clock.now();
			std::cout << "------------------------------------\nTextures loading... ";
			this->Load_Images(path, &LoadedAsset, tp, tpHandles);
			auto end = clock.now();
			std::chrono::duration<double> diff = end - start;
			std::cout << "Completed after: " << diff << "\n";

			start = clock.now();
			std::cout << "------------------------------------\nMeshes loading... ";
			this->Load_And_Meshletize_Meshes(path, &LoadedAsset, tp, tpHandles);
			end = clock.now();
			diff = end - start;
			std::cout << "Completed after: " << diff << "\n";

			start = clock.now();
			std::cout << "------------------------------------\nMaterials loading... ";
			this->Load_Materials(&LoadedAsset);
			end = clock.now();
			diff = end - start;
			std::cout << "Completed after: " << diff << "\n";

			for (auto& handle : tpHandles)
			{
				handle.wait();
			}

			return true;
		}

	private:

		void Load_Images(const std::filesystem::path& path, fastgltf::Asset* asset, aZero::ThreadPool& tp, std::vector<std::future<void>>& handles)
		{
			ImageData.resize(asset->images.size());
			for (int i = 0; i < asset->images.size(); i++)
			{
				handles.emplace_back(tp.AddJobA([this, asset, path, i] {
					ImageData[i] = Asset::ExtractGltfImage(asset->images[i], asset, path);
				}));
			}
		}

		/*void Load_Images(const std::filesystem::path& path, fastgltf::Asset* asset, aZero::ThreadPool& tp)
		{
			std::vector<std::future<void>> handles(asset->images.size());
			std::vector< std::optional<Asset::ImageData>> imageData;
			imageData.resize(asset->images.size());
			for (int i = 0; i < asset->images.size(); i++)
			{
				handles[i] = tp.AddJobA([&] {
					imageData[i] = Asset::ExtractGltfImage(asset->images[i], asset, path);
				});
			}

			for (uint32_t i = 0; i < handles.size(); i++)
			{
				handles[i].wait();
				if (imageData[i] && imageData[i].value().HasValidData())
				{
					ImageIndexToName.emplace(i, imageData[i].value().Name);
					ImageData.emplace_back(std::move(imageData[i].value()));
				}
			}
		}*/

		void Load_And_Meshletize_Meshes(const std::filesystem::path& path, fastgltf::Asset* asset, aZero::ThreadPool& tp, std::vector<std::future<void>>& handles)
		{
			MeshData.resize(asset->meshes.size());

			for (int i = 0; i < asset->meshes.size(); i++)
			{
				handles.emplace_back(tp.AddJobA([this, asset, i] {
					MeshData[i] = Asset::ExtractGltfMeshletMesh(asset->meshes[i], asset);
				}));
			}
		}

		/*void Load_And_Meshletize_Meshes(const std::filesystem::path& path, fastgltf::Asset* asset, aZero::ThreadPool& tp)
		{
			std::vector<std::future<void>> handles(asset->meshes.size());
			MeshData.reserve(asset->meshes.size());

			for (int i = 0; i < asset->meshes.size(); i++)
			{
				handles[i] = tp.AddJobA([&] {
					MeshData[i] = Asset::ExtractGltfMeshletMesh(asset->meshes[i], asset);
					});
			}

			for (uint32_t i = 0; i < handles.size(); i++)
			{
				handles[i].wait();
				if (MeshData[i].HasValidData())
				{
					MeshIndexToName.emplace(i, MeshData[i].Name);
				}
			}
		}*/

		void Load_Materials(fastgltf::Asset* asset)
		{
			for (int i = 0; i < asset->materials.size(); i++)
			{
				Materials.push_back(std::make_pair<std::string, Asset::PBRMaterialData>(asset->materials[i].name.c_str(), ExtractPBRMaterial(asset->materials[i], asset)));
			}
		}
	};
}