#pragma once
#include <bitset>
#include <filesystem>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <flecs.h>
#include "components/TransformComponents.hpp"
#include "components/MeshComponent.hpp"
#include "components/TriggerbodyComponent.hpp"
#include "components/RigidbodyComponent.hpp"
#include "components/LightComponents.hpp"
#include "components/CameraComponent.hpp"
#include "physics/PhysicsWorld.hpp"
#include "misc/LinearAllocator.hpp"

namespace aZero
{
	namespace Physics { class PhysicsEngine; }
	namespace Rendering { class WireframeRenderer; }
	namespace RenderAPI { class CommandList; }
	namespace Component{ class StaticComponent{}; }
	namespace System { class MeshSystem; class MaterialSystem; class TextureSystem; }

	namespace Scene
	{
		struct SceneRenderDataFrameInfo
		{
			uint32_t MeshCount;
			uint32_t PointLightCount, SpotLightCount, DirectionalLightCount;
			std::vector<const aZero::Component::CameraComponent*> Cameras;
		};

		struct SceneRenderData
		{
			// todo Make buffers resizable
			aZero::RenderAPI::Buffer InstanceCullDataBuffer;
			aZero::RenderAPI::Buffer InstanceBuffer;
			aZero::RenderAPI::Buffer CameraBuffer;
			aZero::RenderAPI::Buffer PointLightBuffer;
			aZero::RenderAPI::Buffer SpotLightBuffer;
			aZero::RenderAPI::Buffer DirectionalLightBuffer;
			std::vector<Component::CameraComponent*> Cameras;
		};

		using SceneID = uint32_t;

		class Scene
		{
		public:
			Scene();
			Scene(Physics::PhysicsEngine& physicsEngine);

			// todo Impl move-ops etc...

			~Scene();

			bool LoadGltf(const std::filesystem::path& path);

			flecs::world& GetWorld() { return m_World; }
			SceneID GetSceneID() const { return m_SceneID; }
			flecs::query<> GetRootEntityQuery() const { return m_RootEntityQuery; }
			bool LoadGltf(const std::filesystem::path& path, System::MeshSystem& meshSystem, System::MaterialSystem& matSystem, System::TextureSystem& tSystem);

			flecs::entity GetStaticMeshPrefab() const { return m_StaticMeshPrefab; }
			flecs::entity GetCameraPrefab() const { return m_CameraPrefab; }
			flecs::query<const Component::CameraComponent, const Component::PositionComponent, const Component::RotationComponent> GetCameraQuery() const { return m_CameraQuery; }
			flecs::query<const Component::MeshComponent, const Component::PositionComponent, const Component::RotationComponent, const Component::ScaleComponent> GetStaticMeshQuery() const { return m_Static_Mesh_Query; }
			flecs::query<const Component::MeshComponent, const Component::PositionComponent, const Component::RotationComponent, const Component::ScaleComponent> GetDynamicMeshQuery() const { return m_Dynamic_Mesh_Query; }
			flecs::query<Component::RigidbodyComponent, Component::PositionComponent, Component::RotationComponent> GetRigidbodyQuery() const { return m_ApplyPhysicsQuery; }
			flecs::query<Component::TriggerbodyComponent, Component::PositionComponent> GetTriggerbodyQuery() const { return m_TriggerbodyQuery; }

			std::tuple<SceneRenderDataFrameInfo, std::reference_wrapper<SceneRenderData>> CreateRenderData(RenderAPI::Core& core, Renderer::ResourceRecycler& recycler, aZero::LinearAllocator<>& frameDataAllocator, RenderAPI::Buffer& frameDataBuffer, aZero::RenderAPI::CommandBuffer& commandBuffer);

			bool HasPhysics() const { return m_PhysicsWorld.get() != nullptr; }
			void ApplyPhysics();
			void UpdatePhysics(bool applyImmediate = true);
			void OptimizePhysics();

			void MarkStaticMeshesDirty() { m_ShouldRebuildStaticMeshes = true; }
			void MarkStaticLightsDirty() { m_ShouldRebuildStaticLights = true; }

			const std::unordered_map<uint32_t, flecs::entity_t>& GetBodyID_To_EntityID_Map() const { return m_BodyID_To_EntityID; } // This name...

		private:
			void Init();

			void UploadStaticMeshes(aZero::LinearAllocator<>& frameDataAllocator, RenderAPI::Buffer& frameDataBuffer, RenderAPI::CommandBuffer& commandBuffer);
			uint32_t UploadDynamicMeshes(aZero::LinearAllocator<>& frameDataAllocator, RenderAPI::Buffer& frameDataBuffer, RenderAPI::CommandBuffer& commandBuffer);

			void UploadStaticLights(aZero::LinearAllocator<>& frameDataAllocator, RenderAPI::Buffer& frameDataBuffer, RenderAPI::CommandBuffer& cmdList);
			std::tuple<uint32_t, uint32_t, uint32_t> UploadDynamicLights(aZero::LinearAllocator<>& frameDataAllocator, RenderAPI::Buffer& frameDataBuffer, RenderAPI::CommandBuffer& cmdList);

			std::vector<const Component::CameraComponent*> UploadCameras(aZero::LinearAllocator<>& frameDataAllocator, RenderAPI::Buffer& frameDataBuffer, RenderAPI::CommandBuffer& cmdList);

			void ResolveCollisionEvents();
			bool RegisterToPhysics(flecs::entity entity, Physics::Body& body, JPH::BodyCreationSettings& bodySettings);
			void UnregisterFromPhysics(flecs::entity entity, Physics::Body& body);

			std::unordered_map<uint32_t, std::string> LoadGltf_Meshes(const std::filesystem::path& path, System::MeshSystem& mSystem, fastgltf::Asset* asset);
			std::unordered_map<uint32_t, std::string> LoadGltf_Materials(const std::filesystem::path& path, System::MaterialSystem& matSystem, System::TextureSystem& tSystem, fastgltf::Asset* asset, const std::unordered_map<uint32_t, std::string>& imageIndexToName);

			// Currently only supports images
			std::unordered_map<uint32_t, std::string> LoadGltf_Textures(const std::filesystem::path& path, System::TextureSystem& tSystem, fastgltf::Asset* asset);

			flecs::world m_World;
			flecs::observer m_Rigidbody_OnSet_Observer;
			flecs::observer m_Rigidbody_OnRemove_Observer;
			flecs::observer m_Triggerbody_OnSet_Observer;
			flecs::observer m_Triggerbody_OnRemove_Observer;
			flecs::observer m_ParentTransform_Observer;

			flecs::query<> m_RootEntityQuery;

			flecs::query<const Component::MeshComponent, const Component::PositionComponent, const Component::RotationComponent, const Component::ScaleComponent> m_Static_Mesh_Query;
			flecs::query<const Component::MeshComponent, const Component::PositionComponent, const Component::RotationComponent, const Component::ScaleComponent> m_Dynamic_Mesh_Query;

			flecs::query<const Component::PointLightComponent, const Component::PositionComponent> m_Static_PointLight_Query;
			flecs::query<const Component::PointLightComponent, const Component::PositionComponent> m_Dynamic_PointLight_Query;

			flecs::query<const Component::SpotLightComponent, const Component::PositionComponent, const Component::RotationComponent> m_Static_SpotLight_Query;
			flecs::query<const Component::SpotLightComponent, const Component::PositionComponent, const Component::RotationComponent> m_Dynamic_SpotLight_Query;

			flecs::query<const Component::DirectionalLightComponent, const Component::RotationComponent> m_Static_DirectionalLight_Query;
			flecs::query<const Component::DirectionalLightComponent, const Component::RotationComponent> m_Dynamic_DirectionalLight_Query;

			flecs::query<const Component::CameraComponent, const Component::PositionComponent, const Component::RotationComponent> m_CameraQuery;
			flecs::query<Component::RigidbodyComponent, Component::PositionComponent, Component::RotationComponent> m_ApplyPhysicsQuery;
			flecs::query<Component::TriggerbodyComponent, Component::PositionComponent> m_TriggerbodyQuery;

			flecs::entity m_StaticMeshPrefab;
			flecs::entity m_RigidbodyStaticMeshPrefab;
			flecs::entity m_CameraPrefab;

			std::unique_ptr<Physics::PhysicsWorld> m_PhysicsWorld;
			std::unordered_map<uint32_t, flecs::entity_t> m_BodyID_To_EntityID;

			SceneID m_SceneID;
			static inline std::atomic<SceneID> m_IncrementingID = 0;

			SceneRenderData m_RenderData;
			uint32_t m_NumStaticInstances = 0;

			uint32_t m_NumStaticPointLights = 0;
			uint32_t m_NumStaticSpotLights = 0;
			uint32_t m_NumStaticDirectionalLights = 0;

			bool m_ShouldRebuildStaticMeshes = true;
			bool m_ShouldRebuildStaticLights = true;
		};
	}
}