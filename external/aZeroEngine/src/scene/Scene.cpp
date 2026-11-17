#include "Scene.hpp"
#include "physics/PhysicsEngine.hpp"
#include "renderer/mesh_draw_pipeline/Mesh_Draw_Structs.hpp"
#include "misc/Helpers.hpp"
#include "misc/Maths.hpp"
#include "renderer/system/MeshSystem.hpp"
#include "renderer/system/MaterialSystem.hpp"
#include "renderer/system/TextureSystem.hpp"
#include "asset/material/PBR_Material.hpp"

#include "misc/ThreadPool.hpp"
#include <glm/gtx/euler_angles.hpp>
#include "asset/loading_saving/Gltf.hpp"

glm::mat4x4 CreateWorldMatrix(const glm::vec3 position, const glm::vec3 rotation, const glm::vec3 scale)
{
	glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
	glm::mat4 R = glm::eulerAngleYXZ(glm::radians(rotation.y), glm::radians(rotation.x), glm::radians(rotation.z));
	glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
	return T * R * S;
}

void WriteRenderFormat(aZero::GPUStruct::InstanceCullData* pObjCull, aZero::GPUStruct::InstanceData* pInstance,
	const aZero::Component::MeshComponent& mesh, const aZero::Component::PositionComponent& position, const aZero::Component::RotationComponent& rotation, const aZero::Component::ScaleComponent& scale, uint32_t& instanceDataIndex, uint32_t baseOffsetInstanceIndex)
{
	using namespace aZero::Rendering;
	aZero::GPUStruct::InstanceData instanceData;
	instanceData.WorldMatrix = CreateWorldMatrix(position.Position, rotation.Rotation, scale.Scale);

	for (uint32_t j = 0; j < mesh.m_NumSubmeshes; j++)
	{
		aZero::GPUStruct::InstanceCullData objectCullData;
		objectCullData.WorldSpaceBounds.Origin = instanceData.WorldMatrix * glm::vec4(mesh.m_Submeshes[j].Bounds.Origin, 1.f);
		objectCullData.MeshletCount = mesh.m_Submeshes[j].MeshletCount;
		objectCullData.MaterialTypeIndex = mesh.m_Submeshes[j].MaterialTypeIndex;
		*(pObjCull + instanceDataIndex) = objectCullData;

		instanceData.MaterialPtr = mesh.m_Submeshes[j].MaterialPtr;
		instanceData.MeshletBDA = mesh.m_Submeshes[j].MeshletBufferPtr;
		instanceData.MeshletBoundsBDA = mesh.m_Submeshes[j].MeshletBoundsBufferPtr;
		instanceData.VertexBDA = mesh.m_Submeshes[j].VertexBufferPtr;
		instanceData.MeshletCount = mesh.m_Submeshes[j].MeshletCount;
		*(pInstance + instanceDataIndex) = instanceData;
		instanceDataIndex++;
	}
}

aZero::Scene::Scene::Scene()
{
	this->Init();
}

aZero::Scene::Scene::Scene(Physics::PhysicsEngine& physicsEngine)
	:m_PhysicsWorld(std::make_unique<Physics::PhysicsWorld>())
{
	physicsEngine.CreateWorld(*m_PhysicsWorld.get());
	this->Init();
}

aZero::Scene::Scene::~Scene()
{
	// Reset queries since they should live shorter than the world
	m_Dynamic_Mesh_Query = {};
	m_Static_Mesh_Query = {};
	m_Dynamic_PointLight_Query = {};
	m_Static_PointLight_Query = {};
	m_Dynamic_SpotLight_Query = {};
	m_Static_SpotLight_Query = {};
	m_Dynamic_DirectionalLight_Query = {};
	m_Static_DirectionalLight_Query = {};
	m_CameraQuery = {};
	m_ApplyPhysicsQuery = {};
	m_TriggerbodyQuery = {};
	//m_RootEntityQuery = {};

	if (m_PhysicsWorld.get())
	{
		m_Rigidbody_OnSet_Observer.destruct();
		m_Rigidbody_OnRemove_Observer.destruct();
		m_Triggerbody_OnSet_Observer.destruct();
		m_Triggerbody_OnRemove_Observer.destruct();
		m_World.release(); // Need to be destroyed before the physics world since there's observers that use the physics world (unless they are disabled beforehand)
		m_PhysicsWorld.reset();
	}
}

void aZero::Scene::Scene::Init()
{
	m_SceneID = m_IncrementingID.fetch_add(1, std::memory_order_relaxed);

	m_World.component<Component::PositionComponent>();
	m_World.component<Component::RotationComponent>();
	m_World.component<Component::ScaleComponent>();
	m_World.component<Component::CameraComponent>();
	m_World.component<Component::MeshComponent>();
	m_World.component<Component::PointLightComponent>();
	m_World.component<Component::SpotLightComponent>();
	m_World.component<Component::DirectionalLightComponent>();
	m_World.component<Component::RigidbodyComponent>();
	m_World.component<Component::TriggerbodyComponent>();
	m_World.component<Component::StaticComponent>();

	/*m_RootEntityQuery = m_World.query_builder<>()
		.with<Component::PositionComponent>().oper(flecs::Or)
		.with<Component::RotationComponent>().oper(flecs::Or)
		.with<Component::ScaleComponent>().oper(flecs::Or)
		.with<Component::CameraComponent>().oper(flecs::Or)
		.with<Component::MeshComponent>().oper(flecs::Or)
		.with<Component::PointLightComponent>().oper(flecs::Or)
		.with<Component::SpotLightComponent>().oper(flecs::Or)
		.with<Component::DirectionalLightComponent>().oper(flecs::Or)
		.with<Component::RigidbodyComponent>().oper(flecs::Or)
		.with<Component::TriggerbodyComponent>().oper(flecs::Or)
		.with<Component::StaticComponent>()
		.without(flecs::ChildOf)
		.build();*/

	m_Static_Mesh_Query = m_World.query_builder<const Component::MeshComponent, const Component::PositionComponent, const Component::RotationComponent, const Component::ScaleComponent>().with<Component::StaticComponent>().build();
	m_Dynamic_Mesh_Query = m_World.query_builder<const Component::MeshComponent, const Component::PositionComponent, const Component::RotationComponent, const Component::ScaleComponent>().without<Component::StaticComponent>().cached().build();

	m_Static_PointLight_Query = m_World.query_builder<const Component::PointLightComponent, const Component::PositionComponent>().with<Component::StaticComponent>().build();
	m_Dynamic_PointLight_Query = m_World.query_builder<const Component::PointLightComponent, const Component::PositionComponent>().without<Component::StaticComponent>().cached().build();

	m_Static_SpotLight_Query = m_World.query_builder<const Component::SpotLightComponent, const Component::PositionComponent, const Component::RotationComponent>().with<Component::StaticComponent>().build();
	m_Dynamic_SpotLight_Query = m_World.query_builder<const Component::SpotLightComponent, const Component::PositionComponent, const Component::RotationComponent>().without<Component::StaticComponent>().cached().build();

	m_Static_DirectionalLight_Query = m_World.query_builder<const Component::DirectionalLightComponent, const Component::RotationComponent>().with<Component::StaticComponent>().build();
	m_Dynamic_DirectionalLight_Query = m_World.query_builder<const Component::DirectionalLightComponent, const Component::RotationComponent>().without<Component::StaticComponent>().cached().build();

	m_CameraQuery = m_World.query_builder<
		const Component::CameraComponent,
		const Component::PositionComponent,
		const Component::RotationComponent>()
		.order_by<Component::CameraComponent>(
			[](flecs::entity_t, const Component::CameraComponent* a,
				flecs::entity_t, const Component::CameraComponent* b)
			{
				// High -> low priority
				// -1 -> b lower than a, 0 -> equal, +1 -> a lower than b
				return (a->m_PriorityIndex < b->m_PriorityIndex) - (a->m_PriorityIndex > b->m_PriorityIndex);
			}).cached()
		.build();

	m_StaticMeshPrefab = m_World.prefab("StaticMeshPrefab").set(Component::MeshComponent()).set(Component::PositionComponent(0, 0, 0)).set(Component::RotationComponent(0, 0, 0)).set(Component::ScaleComponent(1, 1, 1));

	Component::CameraComponent cameraPrefab{
						.m_Type = Component::ECameraType::PERSPECTIVE,
						.m_PriorityIndex = 1,
						.m_Fov = M_PI - 1,
						.m_Top = 0.0f,
						.m_Left = 0.0f,
						.m_Bottom = 1.f,
						.m_Right = 1.f,
						.m_Near = 0.001f,
						.m_Far = 1000.f,
						.m_OptColorTarget = std::nullopt,
						.m_OptDepthStencilTarget = std::nullopt,
						.m_ClearColorTargetOnRender = true,
						.m_ClearDepthStencilTargetOnRender = true
	};
	m_CameraPrefab = m_World.prefab("CameraPrefab").set(Component::CameraComponent(cameraPrefab)).set(Component::PositionComponent(0, 0, 0)).set(Component::RotationComponent(0, 0, 0));

	if (m_PhysicsWorld.get()) 
	{
		m_RigidbodyStaticMeshPrefab = m_World.prefab("RigidbodyStaticMeshPrefab").set(Component::RigidbodyComponent()).set(Component::MeshComponent()).set(Component::PositionComponent(0, 0, 0)).set(Component::RotationComponent(0, 0, 0)).set(Component::ScaleComponent(1, 1, 1));

		m_ApplyPhysicsQuery = m_World.query_builder<Component::RigidbodyComponent, Component::PositionComponent, Component::RotationComponent>().without<Component::StaticComponent>().cached().build();
		m_TriggerbodyQuery = m_World.query_builder<Component::TriggerbodyComponent, Component::PositionComponent>().cached().build();

		m_Rigidbody_OnSet_Observer = m_World.observer<Component::RigidbodyComponent>().event(flecs::OnSet).each(
			[this](flecs::entity entity, Component::RigidbodyComponent& rb) {
				this->RegisterToPhysics(entity, rb.GetBody(), rb.m_BodySettings);
			}
		);

		m_Rigidbody_OnRemove_Observer = m_World.observer<Component::RigidbodyComponent>().event(flecs::OnRemove).each(
			[this](flecs::entity entity, Component::RigidbodyComponent& rb) {
				this->UnregisterFromPhysics(entity, rb.GetBody());
			}
		);

		m_Triggerbody_OnSet_Observer = m_World.observer<Component::TriggerbodyComponent>().event(flecs::OnSet).each(
			[this](flecs::entity entity, Component::TriggerbodyComponent& tb) {
				this->RegisterToPhysics(entity, tb.GetBody(), tb.m_BodySettings);
			}
		);

		m_Triggerbody_OnRemove_Observer = m_World.observer<Component::TriggerbodyComponent>().event(flecs::OnRemove).each(
			[this](flecs::entity entity, Component::TriggerbodyComponent& tb) {
				this->UnregisterFromPhysics(entity, tb.GetBody());
			}
		);
	}
}

void aZero::Scene::Scene::UploadStaticMeshes(aZero::LinearAllocator<>& frameDataAllocator, RenderAPI::Buffer& frameDataBuffer, RenderAPI::CommandBuffer& commandBuffer)
{
	// todo Reset stuff, ex. tree, for the new cached data
	// todo Rehash mesh in tree etc
	size_t objectCullDataOffset = 0;
	size_t instanceDataOffset = 0;
	m_Static_Mesh_Query.run([this, &frameDataAllocator, &frameDataBuffer, &commandBuffer, &objectCullDataOffset, &instanceDataOffset] (flecs::iter& it) {
		using namespace Rendering;
		GPUStruct::InstanceCullData* pObjCull = reinterpret_cast<GPUStruct::InstanceCullData*>(frameDataAllocator.Allocate(m_Static_Mesh_Query.count() * Component::MeshComponent::s_MaxNumberOfSubmeshes * sizeof(GPUStruct::InstanceCullData)));
		objectCullDataOffset = frameDataAllocator.GetOffset() - m_Static_Mesh_Query.count() * Component::MeshComponent::s_MaxNumberOfSubmeshes * sizeof(GPUStruct::InstanceCullData);

		GPUStruct::InstanceData* pInstance = reinterpret_cast<GPUStruct::InstanceData*>(frameDataAllocator.Allocate(m_Static_Mesh_Query.count() * Component::MeshComponent::s_MaxNumberOfSubmeshes * sizeof(GPUStruct::InstanceData)));
		instanceDataOffset = frameDataAllocator.GetOffset() - m_Static_Mesh_Query.count() * Component::MeshComponent::s_MaxNumberOfSubmeshes * sizeof(GPUStruct::InstanceData); // This guarantees offset starts after alignment

		uint32_t numStaticInstances = 0;

		while (it.next())
		{
			auto mesh = it.field<const Component::MeshComponent>(0);
			auto position = it.field<const Component::PositionComponent>(1);
			auto rotation = it.field<const Component::RotationComponent>(2);
			auto scale = it.field<const Component::ScaleComponent>(3);

			for (auto i : it)
			{
				WriteRenderFormat(pObjCull, pInstance, mesh[i], position[i], rotation[i], scale[i], numStaticInstances, 0);
			}
		}

		m_NumStaticInstances = numStaticInstances;
	});

	commandBuffer.Copy(frameDataBuffer, objectCullDataOffset, m_RenderData.InstanceCullDataBuffer, 0, m_NumStaticInstances * sizeof(GPUStruct::InstanceCullData));
	commandBuffer.Copy(frameDataBuffer, instanceDataOffset, m_RenderData.InstanceBuffer, 0, m_NumStaticInstances * sizeof(GPUStruct::InstanceData));
}

uint32_t aZero::Scene::Scene::UploadDynamicMeshes(aZero::LinearAllocator<>& frameDataAllocator, RenderAPI::Buffer& frameDataBuffer, RenderAPI::CommandBuffer& commandBuffer)
{
	using namespace Rendering;
	uint32_t entityUpdateCount = m_Dynamic_Mesh_Query.count() * Component::MeshComponent::s_MaxNumberOfSubmeshes;

	GPUStruct::InstanceCullData* pObjCull = reinterpret_cast<GPUStruct::InstanceCullData*>(frameDataAllocator.Allocate(entityUpdateCount * sizeof(GPUStruct::InstanceCullData)));
	size_t objectCullDataOffset = frameDataAllocator.GetOffset() - entityUpdateCount * sizeof(GPUStruct::InstanceCullData);

	GPUStruct::InstanceData* pInstance = reinterpret_cast<GPUStruct::InstanceData*>(frameDataAllocator.Allocate(entityUpdateCount * sizeof(GPUStruct::InstanceData)));
	size_t instanceDataOffset = frameDataAllocator.GetOffset() - entityUpdateCount * sizeof(GPUStruct::InstanceData);

	uint32_t numDynamicInstances = 0;
	m_Dynamic_Mesh_Query.each([pObjCull, pInstance, &numDynamicInstances, this](const Component::MeshComponent& mesh, const Component::PositionComponent& position, const Component::RotationComponent& rotation, const Component::ScaleComponent& scale)
		{
			WriteRenderFormat(pObjCull, pInstance, mesh, position, rotation, scale, numDynamicInstances, m_NumStaticInstances);
		});

	commandBuffer.Copy(frameDataBuffer, objectCullDataOffset, m_RenderData.InstanceCullDataBuffer, m_NumStaticInstances * sizeof(GPUStruct::InstanceCullData), numDynamicInstances * sizeof(GPUStruct::InstanceCullData));
	commandBuffer.Copy(frameDataBuffer, instanceDataOffset, m_RenderData.InstanceBuffer, m_NumStaticInstances * sizeof(GPUStruct::InstanceData), numDynamicInstances * sizeof(GPUStruct::InstanceData));

	return m_NumStaticInstances + numDynamicInstances;
}

void aZero::Scene::Scene::UploadStaticLights(aZero::LinearAllocator<>& frameDataAllocator, RenderAPI::Buffer& frameDataBuffer, RenderAPI::CommandBuffer& commandBuffer)
{
	using namespace Rendering;

	{
		GPUStruct::PointLight* plight = reinterpret_cast<GPUStruct::PointLight*>(frameDataAllocator.Allocate(m_Static_PointLight_Query.count() * sizeof(GPUStruct::PointLight)));
		size_t srcOffset = frameDataAllocator.GetOffset() - m_Static_PointLight_Query.count() * sizeof(GPUStruct::PointLight);
		uint32_t numLights = 0;
		m_Static_PointLight_Query.each([&numLights, &plight](const Component::PointLightComponent& light, const Component::PositionComponent& position) {
			GPUStruct::PointLight l{
				.Color = light.Color,
				.Position = position.Position,
				.Range = light.Range,
				.Falloff = light.Falloff,
				.Intensity = light.Intensity
			};
			*(plight + numLights) = l;
			numLights++;
		});
		if (numLights)
		{
			commandBuffer.Copy(frameDataBuffer, srcOffset, m_RenderData.PointLightBuffer, 0, numLights * sizeof(GPUStruct::PointLight));
		}
		m_NumStaticPointLights = numLights;
	}

	{
		GPUStruct::SpotLight* plight = reinterpret_cast<GPUStruct::SpotLight*>(frameDataAllocator.Allocate(m_Static_SpotLight_Query.count() * sizeof(GPUStruct::SpotLight)));
		size_t srcOffset = frameDataAllocator.GetOffset() - m_Static_SpotLight_Query.count() * sizeof(GPUStruct::SpotLight);
		uint32_t numLights = 0;
		m_Static_SpotLight_Query.each([&numLights, &plight](const Component::SpotLightComponent& light, const Component::PositionComponent& position, const Component::RotationComponent& rotation) {
			GPUStruct::SpotLight l{
				.Color = light.Color,
				.Position = position.Position,
				.Direction = rotation.Rotation,
				.Intensity = light.Intensity,
				.Radius = light.Radius
			};
			*(plight + numLights) = l;
			numLights++;
		});
		if (numLights)
		{
			commandBuffer.Copy(frameDataBuffer, srcOffset, m_RenderData.SpotLightBuffer, 0, numLights * sizeof(GPUStruct::SpotLight));
		}
		m_NumStaticSpotLights = numLights;
	}

	{
		GPUStruct::DirectionalLight* plight = reinterpret_cast<GPUStruct::DirectionalLight*>(frameDataAllocator.Allocate(m_Static_DirectionalLight_Query.count() * sizeof(GPUStruct::DirectionalLight)));
		size_t srcOffset = frameDataAllocator.GetOffset() - m_Static_DirectionalLight_Query.count() * sizeof(GPUStruct::DirectionalLight);
		uint32_t numLights = 0;
		m_Static_DirectionalLight_Query.each([&numLights, &plight](const Component::DirectionalLightComponent& light, const Component::RotationComponent& rotation) {
			GPUStruct::DirectionalLight l{
				.Color = light.Color,
				.Direction = rotation.Rotation,
				.Intensity = light.Intensity,
			};
			*(plight + numLights) = l;
			numLights++;
			});
		if (numLights)
		{
			commandBuffer.Copy(frameDataBuffer, srcOffset, m_RenderData.DirectionalLightBuffer, 0, numLights * sizeof(GPUStruct::DirectionalLight));
		}
		m_NumStaticDirectionalLights = numLights;
	}
}

std::tuple<uint32_t, uint32_t, uint32_t> aZero::Scene::Scene::UploadDynamicLights(aZero::LinearAllocator<>& frameDataAllocator, RenderAPI::Buffer& frameDataBuffer, RenderAPI::CommandBuffer& commandBuffer)
{
	using namespace Rendering;

	uint32_t numPointLights = 0, numSpotLights = 0, numDirectionalLights = 0;
	{
		GPUStruct::PointLight* plight = reinterpret_cast<GPUStruct::PointLight*>(frameDataAllocator.Allocate(m_Dynamic_PointLight_Query.count() * sizeof(GPUStruct::PointLight)));
		size_t srcOffset = frameDataAllocator.GetOffset() - m_Dynamic_PointLight_Query.count() * sizeof(GPUStruct::PointLight);
		m_Dynamic_PointLight_Query.each([&numPointLights, &plight](const Component::PointLightComponent& light, const Component::PositionComponent& position) {
			GPUStruct::PointLight l{
				.Color = light.Color,
				.Position = position.Position,
				.Range = light.Range,
				.Falloff = light.Falloff,
				.Intensity = light.Intensity
			};
			*(plight + numPointLights) = l;
			numPointLights++;
			});
		if (numPointLights)
		{
			commandBuffer.Copy(frameDataBuffer, srcOffset, m_RenderData.PointLightBuffer, m_NumStaticPointLights * sizeof(GPUStruct::PointLight), numPointLights * sizeof(GPUStruct::PointLight));
		}
	}

	{
		GPUStruct::SpotLight* plight = reinterpret_cast<GPUStruct::SpotLight*>(frameDataAllocator.Allocate(m_Dynamic_SpotLight_Query.count() * sizeof(GPUStruct::SpotLight)));
		size_t srcOffset = frameDataAllocator.GetOffset() - m_Dynamic_SpotLight_Query.count() * sizeof(GPUStruct::SpotLight);
		m_Dynamic_SpotLight_Query.each([&numSpotLights, &plight](const Component::SpotLightComponent& light, const Component::PositionComponent& position, const Component::RotationComponent& rotation) {
			GPUStruct::SpotLight l{
				.Color = light.Color,
				.Position = position.Position,
				.Direction = rotation.Rotation,
				.Intensity = light.Intensity,
				.Radius = light.Radius
			};
			*(plight + numSpotLights) = l;
			numSpotLights++;
			});
		if (numSpotLights)
		{
			commandBuffer.Copy(frameDataBuffer, srcOffset, m_RenderData.SpotLightBuffer, m_NumStaticSpotLights * sizeof(GPUStruct::SpotLight), numSpotLights * sizeof(GPUStruct::SpotLight));
		}
	}

	{
		GPUStruct::DirectionalLight* plight = reinterpret_cast<GPUStruct::DirectionalLight*>(frameDataAllocator.Allocate(m_Dynamic_DirectionalLight_Query.count() * sizeof(GPUStruct::DirectionalLight)));
		size_t srcOffset = frameDataAllocator.GetOffset() - m_Dynamic_DirectionalLight_Query.count() * sizeof(GPUStruct::DirectionalLight);
		m_Dynamic_DirectionalLight_Query.each([&numDirectionalLights, &plight](const Component::DirectionalLightComponent& light, const Component::RotationComponent& rotation) {
			GPUStruct::DirectionalLight l{
				.Color = light.Color,
				.Direction = rotation.Rotation,
				.Intensity = light.Intensity,
			};
			*(plight + numDirectionalLights) = l;
			numDirectionalLights++;
			});
		if (numDirectionalLights)
		{
			commandBuffer.Copy(frameDataBuffer, srcOffset, m_RenderData.DirectionalLightBuffer, m_NumStaticDirectionalLights * sizeof(GPUStruct::DirectionalLight), numDirectionalLights * sizeof(GPUStruct::DirectionalLight));
		}
	}

	numPointLights = numPointLights + m_NumStaticPointLights;
	numSpotLights = numSpotLights + m_NumStaticSpotLights;
	numDirectionalLights = numDirectionalLights + m_NumStaticDirectionalLights;
	return { numPointLights, numSpotLights, numDirectionalLights };
}


std::vector<const aZero::Component::CameraComponent*> aZero::Scene::Scene::UploadCameras(aZero::LinearAllocator<>& frameDataAllocator, RenderAPI::Buffer& frameDataBuffer, RenderAPI::CommandBuffer& commandBuffer)
{
	uint32_t cameraSrcOffset = frameDataAllocator.GetOffset();
	GPUStruct::CameraData* pCameraData = reinterpret_cast<GPUStruct::CameraData*>(frameDataAllocator.Allocate(m_CameraQuery.count() * sizeof(GPUStruct::CameraData)));

	std::vector<const Component::CameraComponent*> activeCameras;
	activeCameras.reserve(m_CameraQuery.count());
	m_CameraQuery.each([this, &activeCameras, &pCameraData](const Component::CameraComponent& camera, const Component::PositionComponent& position, const Component::RotationComponent& rotation)
		{
			if (camera.m_IsActive)
			{
				GPUStruct::CameraData cameraData;
				cameraData.ViewMatrix = camera.CreateViewMatrix(position.Position, rotation.Rotation);
				cameraData.ViewProjectionMatrix = camera.CreateProjectionMatrix() * cameraData.ViewMatrix;
				cameraData.Frustum = camera.CreateFrustra(position.Position, rotation.Rotation);
				activeCameras.emplace_back(&camera);
				*pCameraData = cameraData;
				pCameraData++;
			}
		});
	commandBuffer.Copy(frameDataBuffer, cameraSrcOffset, m_RenderData.CameraBuffer, 0, activeCameras.size() * sizeof(GPUStruct::CameraData));

	return activeCameras;
}

std::tuple<aZero::Scene::SceneRenderDataFrameInfo, std::reference_wrapper<aZero::Scene::SceneRenderData>> aZero::Scene::Scene::CreateRenderData(RenderAPI::Core& core, Renderer::ResourceRecycler& recycler, aZero::LinearAllocator<>& frameDataAllocator, RenderAPI::Buffer& frameDataBuffer, RenderAPI::CommandBuffer& commandBuffer)
{
	using namespace Rendering;

	// todo Handle if no space

	if (!m_RenderData.InstanceCullDataBuffer.IsInitialized())
	{
		RenderAPI::Buffer::Desc desc{
					.Size = 10000 * sizeof(GPUStruct::InstanceCullData),
					.UsageBits =
						VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
						VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT |
						VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
					.MemoryType = RenderAPI::EMemoryType::VRAM,
					.SeperateHeap = true,
					.Name = "m_RenderData.ObjectCullDataBuffer",
					.OptResourceRecycler = recycler
		};

		m_RenderData.InstanceCullDataBuffer = RenderAPI::Buffer(core, desc);

		desc.Name = "m_RenderData.InstanceBuffer";
		desc.Size = 10000 * sizeof(GPUStruct::InstanceData);
		m_RenderData.InstanceBuffer = RenderAPI::Buffer(core, desc);

		desc.Name = "m_RenderData.CameraBuffer";
		desc.Size = 1000 * sizeof(GPUStruct::CameraData);
		desc.UsageBits =
			VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
			VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		m_RenderData.CameraBuffer = RenderAPI::Buffer(core, desc);

		desc.Name = "m_RenderData.PointLightBuffer";
		desc.Size = 1000 * sizeof(GPUStruct::PointLight);
		desc.UsageBits =
			VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		m_RenderData.PointLightBuffer = RenderAPI::Buffer(core, desc);

		desc.Name = "m_RenderData.SpotLightBuffer";
		desc.Size = 1000 * sizeof(GPUStruct::SpotLight);
		desc.UsageBits =
			VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		m_RenderData.SpotLightBuffer = RenderAPI::Buffer(core, desc);

		desc.Name = "m_RenderData.DirectionalLightBuffer";
		desc.Size = 100 * sizeof(GPUStruct::SpotLight);
		desc.UsageBits =
			VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VkBufferUsageFlagBits::VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		m_RenderData.DirectionalLightBuffer = RenderAPI::Buffer(core, desc);
	}

	std::vector<VkBufferMemoryBarrier2> barriers;

	// Pre-copy barriers
	{
		aZero::RenderAPI::Buffer::TransitionDesc trDesc{
						.DstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
						.DstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
						.Offset = 0,
						.Size = m_RenderData.InstanceCullDataBuffer.GetDesc().Size
		};
		m_RenderData.InstanceCullDataBuffer.Transition(trDesc);

		trDesc.Size = m_RenderData.InstanceBuffer.GetDesc().Size;
		m_RenderData.InstanceBuffer.Transition(trDesc);

		barriers.push_back(m_RenderData.InstanceCullDataBuffer.GetTransition());
		barriers.push_back(m_RenderData.InstanceBuffer.GetTransition());
	}

	{
		aZero::RenderAPI::Buffer::TransitionDesc trDesc{
						.DstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
						.DstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
						.Offset = 0,
						.Size = m_RenderData.CameraBuffer.GetDesc().Size
		};
		m_RenderData.CameraBuffer.Transition(trDesc);
		barriers.push_back(m_RenderData.CameraBuffer.GetTransition());
	}

	{
		aZero::RenderAPI::Buffer::TransitionDesc trDesc{
						.DstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
						.DstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
						.Offset = 0,
						.Size = m_RenderData.PointLightBuffer.GetDesc().Size
		};
		m_RenderData.PointLightBuffer.Transition(trDesc);
		barriers.push_back(m_RenderData.PointLightBuffer.GetTransition());
	}

	{
		aZero::RenderAPI::Buffer::TransitionDesc trDesc{
						.DstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
						.DstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
						.Offset = 0,
						.Size = m_RenderData.SpotLightBuffer.GetDesc().Size
		};
		m_RenderData.SpotLightBuffer.Transition(trDesc);
		barriers.push_back(m_RenderData.SpotLightBuffer.GetTransition());
	}

	{
		aZero::RenderAPI::Buffer::TransitionDesc trDesc{
						.DstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
						.DstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
						.Offset = 0,
						.Size = m_RenderData.DirectionalLightBuffer.GetDesc().Size
		};
		m_RenderData.DirectionalLightBuffer.Transition(trDesc);
		barriers.push_back(m_RenderData.DirectionalLightBuffer.GetTransition());
	}
	
	commandBuffer.Barrier({}, {}, barriers);
	//

	// Copy stage
	if (m_ShouldRebuildStaticMeshes)
	{
		this->UploadStaticMeshes(frameDataAllocator, frameDataBuffer, commandBuffer);
		m_ShouldRebuildStaticMeshes = false;
	}
	uint32_t totalMeshCount = this->UploadDynamicMeshes(frameDataAllocator, frameDataBuffer, commandBuffer);
	auto activeCameras = this->UploadCameras(frameDataAllocator, frameDataBuffer, commandBuffer);

	if (m_ShouldRebuildStaticLights)
	{
		this->UploadStaticLights(frameDataAllocator, frameDataBuffer, commandBuffer);
		m_ShouldRebuildStaticLights = false;
	}
	const auto [numPointLights, numSpotLights, numDirectionalLights] = this->UploadDynamicLights(frameDataAllocator, frameDataBuffer, commandBuffer);

	// Post-copy barriers
	barriers.clear();

	{
		aZero::RenderAPI::Buffer::TransitionDesc trDesc{
						.DstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						.DstAccessMask = VK_ACCESS_2_UNIFORM_READ_BIT,
						.Offset = 0,
						.Size = m_RenderData.CameraBuffer.GetDesc().Size
		};
		m_RenderData.CameraBuffer.Transition(trDesc);
		barriers.push_back(m_RenderData.CameraBuffer.GetTransition());
	}

	{
		aZero::RenderAPI::Buffer::TransitionDesc trDesc{
						.DstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
						.DstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
						.Offset = 0,
						.Size = m_RenderData.InstanceCullDataBuffer.GetDesc().Size
		};
		m_RenderData.InstanceCullDataBuffer.Transition(trDesc);
		barriers.push_back(m_RenderData.InstanceCullDataBuffer.GetTransition());
	}

	{
		aZero::RenderAPI::Buffer::TransitionDesc trDesc{
						   .DstStageMask = VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT,
						   .DstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
						   .Offset = 0,
						   .Size = m_RenderData.InstanceBuffer.GetDesc().Size
		};
		m_RenderData.InstanceBuffer.Transition(trDesc);
		barriers.push_back(m_RenderData.InstanceBuffer.GetTransition());
	}

	// We insert a barrier for the next compute shader for pointlights and spotlights since we'll add clustered or tiled shading later and'll be reading them there too
	{
		aZero::RenderAPI::Buffer::TransitionDesc trDesc{
						.DstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
						.DstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
						.Offset = 0,
						.Size = m_RenderData.PointLightBuffer.GetDesc().Size
		};
		m_RenderData.PointLightBuffer.Transition(trDesc);
		barriers.push_back(m_RenderData.PointLightBuffer.GetTransition());
	}

	{
		aZero::RenderAPI::Buffer::TransitionDesc trDesc{
						.DstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
						.DstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
						.Offset = 0,
						.Size = m_RenderData.SpotLightBuffer.GetDesc().Size
		};
		m_RenderData.SpotLightBuffer.Transition(trDesc);
		barriers.push_back(m_RenderData.SpotLightBuffer.GetTransition());
	}
	//

	{
		aZero::RenderAPI::Buffer::TransitionDesc trDesc{
						.DstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
						.DstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
						.Offset = 0,
						.Size = m_RenderData.DirectionalLightBuffer.GetDesc().Size
		};
		m_RenderData.DirectionalLightBuffer.Transition(trDesc);
		barriers.push_back(m_RenderData.DirectionalLightBuffer.GetTransition());
	}

	commandBuffer.Barrier({}, {}, { barriers });
	//

	return { SceneRenderDataFrameInfo{
		.MeshCount = totalMeshCount,
		.PointLightCount = numPointLights, .SpotLightCount = numSpotLights, .DirectionalLightCount = numDirectionalLights,
		.Cameras = std::move(activeCameras)
	}, m_RenderData };
}

void aZero::Scene::Scene::ApplyPhysics()
{
	if (m_PhysicsWorld.get())
	{
		this->ResolveCollisionEvents();

		m_ApplyPhysicsQuery.each([](Component::RigidbodyComponent& rigidBody, Component::PositionComponent& position, Component::RotationComponent& rotation) {
			auto lock = rigidBody.m_Body.LockForRead();
			if (lock.Succeeded())
			{
				auto& body = lock.GetBody();
				position = Math::Convert(body.GetPosition());
				rotation = glm::eulerAngles(Math::Convert(body.GetRotation()));
			}
		});
	}
}

void aZero::Scene::Scene::UpdatePhysics(bool applyImmediate)
{
	if (m_PhysicsWorld.get())
	{
		m_PhysicsWorld->Update();
		if (applyImmediate) {
			this->ApplyPhysics();
		}
	}
}

void aZero::Scene::Scene::OptimizePhysics()
{
	if (m_PhysicsWorld.get())
	{
		m_PhysicsWorld->OptimizeBroadPhase();
	}
}

bool aZero::Scene::Scene::RegisterToPhysics(flecs::entity entity, Physics::Body& body, JPH::BodyCreationSettings& bodySettings)
{
	if (m_BodyID_To_EntityID.count(body.GetBodyID().GetIndexAndSequenceNumber()))
	{
		this->UnregisterFromPhysics(entity, body);
	}

	bodySettings.mUserData = static_cast<uint64_t>(entity.id());
	auto res = m_PhysicsWorld->CreateBody(bodySettings, true);
	if (res)
	{
		body = std::move(res.value());
		m_BodyID_To_EntityID[body.GetBodyID().GetIndexAndSequenceNumber()] = entity.id();
	}
	return res.has_value();
}

void aZero::Scene::Scene::UnregisterFromPhysics(flecs::entity entity, Physics::Body& body)
{
	if (m_BodyID_To_EntityID.count(body.GetBodyID().GetIndexAndSequenceNumber()))
	{
		m_BodyID_To_EntityID.erase(body.GetBodyID().GetIndexAndSequenceNumber());
		m_PhysicsWorld->DestroyBody(body);
	}
}

void aZero::Scene::Scene::ResolveCollisionEvents()
{
	if (m_PhysicsWorld.get())
	{
		for (auto& event : m_PhysicsWorld->GetBodyActivatedEvents()) {
			flecs::entity entity = m_World.entity(event.EntityID);
			if (entity.is_alive() && entity.has<Component::RigidbodyComponent>())
			{
				Component::RigidbodyComponent& rb = entity.get_mut<Component::RigidbodyComponent>();
				if (rb.m_OnBodyActivated.has_value()) {
					rb.m_OnBodyActivated.value()();
				}
			}
		}

		for (auto& event : m_PhysicsWorld->GetBodyDeactivatedEvents()) {
			flecs::entity entity = m_World.entity(event.EntityID);
			if (entity.is_alive())
			{
				if (entity.has<Component::RigidbodyComponent>())
				{
					Component::RigidbodyComponent& rb = entity.get_mut<Component::RigidbodyComponent>();
					if (rb.m_OnBodyDeactivated.has_value()) {
						rb.m_OnBodyDeactivated.value()();
					}
				}
			}
		}

		// For both bodies in a collision we call the collision functions with the other body as an argument
		for (auto& event : m_PhysicsWorld->GetContactValidateEvents()) {
			flecs::entity firstEntity = m_World.entity(event.FirstEntityID);
			flecs::entity secondEntity = m_World.entity(event.SecondEntityID);
			if (firstEntity.is_alive() && secondEntity.is_alive())
			{
				if (firstEntity.has<Component::RigidbodyComponent>() && secondEntity.has<Component::RigidbodyComponent>())
				{
					Component::RigidbodyComponent& firstRb = firstEntity.get_mut<Component::RigidbodyComponent>();
					Component::RigidbodyComponent& secondRb = secondEntity.get_mut<Component::RigidbodyComponent>();

					if (firstRb.m_OnContactValidate.has_value()) {
						firstRb.m_OnContactValidate.value()(secondRb.m_Body, event.InBaseOffset, event.InCollisionResult);
					}

					if (secondRb.m_OnContactValidate.has_value()) {
						secondRb.m_OnContactValidate.value()(firstRb.m_Body, event.InBaseOffset, event.InCollisionResult);
					}
				}
			}
		}

		for (auto& event : m_PhysicsWorld->GetContactAddedEvents()) {
			flecs::entity firstEntity = m_World.entity(event.FirstEntityID);
			flecs::entity secondEntity = m_World.entity(event.SecondEntityID);
			if (firstEntity.is_alive() && secondEntity.is_alive())
			{
				if (firstEntity.has<Component::RigidbodyComponent>() && secondEntity.has<Component::RigidbodyComponent>())
				{
					Component::RigidbodyComponent& firstRb = firstEntity.get_mut<Component::RigidbodyComponent>();
					Component::RigidbodyComponent& secondRb = secondEntity.get_mut<Component::RigidbodyComponent>();

					if (firstRb.m_OnContactAdded.has_value()) {
						firstRb.m_OnContactAdded.value()(secondRb.m_Body, event.ContactManifold, event.ContactSettings);
					}

					if (secondRb.m_OnContactAdded.has_value()) {
						secondRb.m_OnContactAdded.value()(firstRb.m_Body, event.ContactManifold, event.ContactSettings);
					}
				}
			}
		}

		for (auto& event : m_PhysicsWorld->GetContactPersistedEvents()) {
			flecs::entity firstEntity = m_World.entity(event.FirstEntityID);
			flecs::entity secondEntity = m_World.entity(event.SecondEntityID);

			if (firstEntity.is_alive() && secondEntity.is_alive())
			{
				if (firstEntity.has<Component::RigidbodyComponent>() && secondEntity.has<Component::RigidbodyComponent>())
				{
					Component::RigidbodyComponent& firstRb = firstEntity.get_mut<Component::RigidbodyComponent>();
					Component::RigidbodyComponent& secondRb = secondEntity.get_mut<Component::RigidbodyComponent>();

					if (firstRb.m_OnContactPersisted.has_value()) {
						firstRb.m_OnContactPersisted.value()(secondRb.m_Body, event.ContactManifold, event.ContactSettings);
					}

					if (secondRb.m_OnContactPersisted.has_value()) {
						secondRb.m_OnContactPersisted.value()(firstRb.m_Body, event.ContactManifold, event.ContactSettings);
					}
				}
			}
		}

		for (auto& event : m_PhysicsWorld->GetContactRemovedEvents()) {
			if (m_BodyID_To_EntityID.count(event.InSubShapePair.GetBody1ID().GetIndexAndSequenceNumber()) && m_BodyID_To_EntityID.count(event.InSubShapePair.GetBody2ID().GetIndexAndSequenceNumber()))
			{
				flecs::entity firstEntity = m_World.entity(m_BodyID_To_EntityID.at(event.InSubShapePair.GetBody1ID().GetIndexAndSequenceNumber()));
				flecs::entity secondEntity = m_World.entity(m_BodyID_To_EntityID.at(event.InSubShapePair.GetBody2ID().GetIndexAndSequenceNumber()));

				if (firstEntity.has<Component::RigidbodyComponent>() && secondEntity.has<Component::RigidbodyComponent>())
				{
					Component::RigidbodyComponent& firstRb = firstEntity.get_mut<Component::RigidbodyComponent>();
					Component::RigidbodyComponent& secondRb = secondEntity.get_mut<Component::RigidbodyComponent>();

					if (firstRb.m_OnContactRemoved.has_value()) {
						firstRb.m_OnContactRemoved.value()(event.InSubShapePair);
					}

					if (secondRb.m_OnContactRemoved.has_value()) {
						secondRb.m_OnContactRemoved.value()(event.InSubShapePair);
					}
				}
			}
		}
		//

		m_PhysicsWorld->ResetBodyEvents();
	}
}

std::unordered_map<uint32_t, std::string> aZero::Scene::Scene::LoadGltf_Meshes(const std::filesystem::path& path, System::MeshSystem& mSystem, fastgltf::Asset* asset)
{
	// todo Lazy :)))) Fix this
	aZero::ThreadPool tp(std::thread::hardware_concurrency());

	std::unordered_map<uint32_t, std::string> meshIndexToName;

	std::vector<std::future<void>> handles(asset->meshes.size());
	std::vector<Asset::MeshletMeshData> meshDataVec(asset->meshes.size());

	for (int c = 0; c < asset->meshes.size(); c++)
	{
		handles[c] = tp.AddJobA([c, asset, &meshDataVec, &path] {
			const fastgltf::Mesh& mesh = asset->meshes[c];
			const uint32_t numPrimitives = std::clamp((uint32_t)mesh.primitives.size(), 0u, Component::MeshComponent::s_MaxNumberOfSubmeshes); // Currently only supports atmost 10 submeshes
			Asset::MeshletMeshData& meshData = meshDataVec[c];
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
					vertexData[idx].Normal = Asset::PackNormal(Helpers::EncodeNormalOctahedral(glm::vec3(n.x(), n.y(), n.z())));
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
		});
	}

	std::cout << "Waiting on mesh loading + meshlet generation...\n";
	for (auto& handle : handles)
	{
		handle.wait();
	}
	std::cout << "Creating mesh resources...\n";

	for (int c = 0; c < asset->meshes.size(); c++)
	{
		Asset::MeshletMeshData& meshData = meshDataVec[c];
		std::string name = meshData.Name;
		auto res = mSystem.CreateMesh(name, std::move(meshData));
		if (res)
		{
			meshIndexToName[c] = name;
		}
	}

	return meshIndexToName;
}

std::unordered_map<uint32_t, std::string> aZero::Scene::Scene::LoadGltf_Materials(const std::filesystem::path& path, System::MaterialSystem& matSystem, System::TextureSystem& tSystem, fastgltf::Asset* asset, const std::unordered_map<uint32_t, std::string>& imageIndexToName)
{
	std::unordered_map<uint32_t, std::string> materialIndexToName;
	auto nullTexture = tSystem.GetTexture(System::TextureSystem::s_Null_TextureStr);
	auto pbrMaterialTemplate = matSystem.GetMaterial("PBR");
	if (!pbrMaterialTemplate || !nullTexture)
	{
		return materialIndexToName;
	}

	for (int i = 0; i < asset->materials.size(); i++)
	{
		const fastgltf::Material& material = asset->materials[i];

		auto materialInstance = matSystem.CreateMaterialInstance(material.name.c_str(), pbrMaterialTemplate.value().get());
		if (!materialInstance)
		{
			continue;
		}

		Asset::PBRMaterialData pbrData;

		if (material.pbrData.baseColorTexture.has_value()) {
			auto imageIndex = asset->textures[material.pbrData.baseColorTexture.value().textureIndex].imageIndex.has_value() ? 
				asset->textures[material.pbrData.baseColorTexture.value().textureIndex].imageIndex.value() : 
				asset->textures[material.pbrData.baseColorTexture.value().textureIndex].basisuImageIndex.has_value() ? asset->textures[material.pbrData.baseColorTexture.value().textureIndex].basisuImageIndex.value() :
				std::numeric_limits<uint32_t>::max();

			if (auto iter = imageIndexToName.find(imageIndex); iter != imageIndexToName.end()) {
				auto res = tSystem.GetTexture(iter->second);
				if (res)
				{
					pbrData.AlbedoTexture = res.value().get().GetDescriptorIndex();
				}
				else
				{
					pbrData.AlbedoTexture = nullTexture.value().get().GetDescriptorIndex();
				}
			}
		}

		if (material.normalTexture.has_value()) {
			auto imageIndex = asset->textures[material.normalTexture.value().textureIndex].imageIndex.has_value() ?
				asset->textures[material.normalTexture.value().textureIndex].imageIndex.value() :
				asset->textures[material.normalTexture.value().textureIndex].basisuImageIndex.has_value() ? asset->textures[material.normalTexture.value().textureIndex].basisuImageIndex.value() :
				std::numeric_limits<uint32_t>::max();

			if (auto iter = imageIndexToName.find(imageIndex); iter != imageIndexToName.end()) {
				auto res = tSystem.GetTexture(iter->second);
				if (res)
				{
					pbrData.NormalMap = res.value().get().GetDescriptorIndex();
				}
				else
				{
					pbrData.NormalMap = nullTexture.value().get().GetDescriptorIndex();
				}
			}
		}

		if (material.pbrData.metallicRoughnessTexture.has_value()) {
			// todo Impl
			pbrData.RoughnessMetallic = nullTexture.value().get().GetDescriptorIndex();
		}

		pbrData.RoughnessFactor = material.pbrData.roughnessFactor;
		pbrData.MetallicFactor = material.pbrData.metallicFactor;

		materialIndexToName[i] = material.name;

		materialInstance->get().RawCopy((void*)&pbrData, sizeof(pbrData));
	}

	return materialIndexToName;
}

std::unordered_map<uint32_t, std::string> aZero::Scene::Scene::LoadGltf_Textures(const std::filesystem::path& path, System::TextureSystem& tSystem, fastgltf::Asset* asset)
{
	// todo Lazy :)))) Fix this
	aZero::ThreadPool tp(std::thread::hardware_concurrency());

	std::unordered_map<uint32_t, std::string> imageIndexToName;

	std::vector<std::future<void>> handles(asset->images.size());

	std::vector<Asset::ImageData> textureDataVec(asset->images.size());
	for (int i = 0; i < asset->images.size(); i++)
	{
		handles[i] = tp.AddJobA([i, asset, &textureDataVec, &path, &tSystem] {
			Asset::ImageData& textureData = textureDataVec[i];
			const fastgltf::Image& image = asset->images[i];

			std::visit(fastgltf::visitor{
				[](const auto& arg) {},
				[&textureData, &path, &tSystem](const fastgltf::sources::URI& filePath) {
					// todo Figure out how to handle formatting
					std::filesystem::path relative(filePath.uri.path());
					auto res = aZero::Asset::LoadImageData(path.parent_path() / relative, true);
					if (res && res.value().HasValidData())
					{
						textureData = std::move(res.value());
					}
				},
				[&textureData, &image](const fastgltf::sources::Array& vector) {
					auto res = aZero::Asset::LoadImageDataFromMemory(image.name.c_str(), vector.bytes.data(), vector.bytes.size(), 0);
					if (res.HasValidData())
					{
						textureData = std::move(res);
					}
				},
				[asset, &textureData, &image](const fastgltf::sources::BufferView& view) {
					auto& bufferView = asset->bufferViews[view.bufferViewIndex];
					auto& buffer = asset->buffers[bufferView.bufferIndex];
					std::visit(fastgltf::visitor{
						[](const auto& arg) {},
						[&](const fastgltf::sources::Array& vector) {
							auto res = aZero::Asset::LoadImageDataFromMemory(image.name.c_str(), vector.bytes.data(), bufferView.byteLength, bufferView.byteOffset);
							if (res.HasValidData())
							{
								textureData = std::move(res);
							}
						}
					}, buffer.data);
				}
				}, image.data);
		});
	}

	std::cout << "Waiting on texture loading + decoding...\n";
	for (auto& handle : handles)
	{
		handle.wait();
	}
	std::cout << "Creating texture resources...\n";
	/*for (size_t i = 0; i < textureDataVec.size(); i++)
	{
		const auto& data = textureDataVec[i];

		std::cout
			<< "Image " << i
			<< " | Width: " << data.BaseWidth
			<< " | Height: " << data.BaseHeight
			<< " | Data size: " << data.Data.size()
			<< " | Mips: " << data.MipLevels.size()
			<< " | Valid: " << data.HasValidData()
			<< '\n';
	}*/
	for (int i = 0; i < asset->images.size(); i++)
	{
		Asset::ImageData& textureData = textureDataVec[i];
		std::string name = textureData.Name;
		auto res = tSystem.CreateTexture(name, std::move(textureData));
		if (res)
		{
			imageIndexToName[i] = name;
			//tex->ClearCachedData();
		}
	}

	return imageIndexToName;
}

bool aZero::Scene::Scene::FromGltf(Asset::LoadedGltf& gltf, System::MeshSystem& meshSystem, System::MaterialSystem& matSystem, System::TextureSystem& tSystem, bool consumeGltf)
{
	auto nullTexture = tSystem.GetTexture(System::TextureSystem::s_Null_TextureStr);
	auto pbrMaterialTemplate = matSystem.GetMaterial(System::MaterialSystem::s_PBRTypeStr);
	auto defaultPbrInstance = matSystem.GetMaterialInstance(System::MaterialSystem::s_DefaultPBRInstanceStr);
	if (!pbrMaterialTemplate || !nullTexture || !defaultPbrInstance)
	{
		return false;
	}

	// Mesh creation
	std::unordered_map<uint32_t, std::string> meshIndexToName;
	for (uint32_t i = 0; i < gltf.MeshData.size(); i++)
	{
		Asset::MeshletMeshData& meshData = gltf.MeshData[i];
		std::optional<std::reference_wrapper<Asset::Mesh>> res;
		std::string name = meshData.Name;
		if (consumeGltf)
		{
			res = meshSystem.CreateMesh(name, std::move(meshData));
		}
		else
		{
			res = meshSystem.CreateMesh(name, meshData);
		}

		if (res)
		{
			meshIndexToName[i] = name;
		}
		else // todo Impl loading s_DefaultMeshStr
		{
			meshIndexToName[i] = System::MeshSystem::s_DefaultMeshStr;
		}
	}
	//

	// Texture creation
	std::unordered_map<uint32_t, std::string> imageIndexToName;
	for (uint32_t i = 0; i < gltf.ImageData.size(); i++)
	{
		auto& imageData = gltf.ImageData[i];
		if (!imageData)
		{
			imageIndexToName[i] = System::TextureSystem::s_Null_TextureStr;
			continue;
		}

		std::optional<std::reference_wrapper<Asset::Texture>> res;
		std::string name = imageData.value().Name;
		if (consumeGltf)
		{
			res = tSystem.CreateTexture(name, std::move(imageData.value()));
		}
		else
		{
			res = tSystem.CreateTexture(name, imageData.value());
		}

		if (res)
		{
			imageIndexToName[i] = name;
		}
		else
		{
			imageIndexToName[i] = System::TextureSystem::s_Null_TextureStr;
		}
	}
	//

	// Material creation
	auto assignTexture = [&imageIndexToName, &nullTexture, &tSystem](uint32_t& textureIndex) {
		if (textureIndex != std::numeric_limits<uint32_t>::max())
		{
			if (auto iter = imageIndexToName.find(textureIndex); iter != imageIndexToName.end())
			{
				auto tex = tSystem.GetTexture(iter->second);
				if (tex)
				{
					textureIndex = tex->get().GetDescriptorIndex();
				}
				else
				{
					textureIndex = nullTexture->get().GetDescriptorIndex();
				}
			}
		}
	};

	std::unordered_map<uint32_t, std::string> materialIndexToName;
	for (uint32_t i = 0; i < gltf.Materials.size(); i++)
	{
		auto& [name, materialData] = gltf.Materials[i];
		assignTexture(materialData.AlbedoTexture);
		assignTexture(materialData.NormalMap);
		assignTexture(materialData.RoughnessMetallic);
		assignTexture(materialData.GlowMap);
		assignTexture(materialData.TransparencyMap);

		auto materialInstance = matSystem.CreateMaterialInstance(name, pbrMaterialTemplate.value().get());
		if (!materialInstance)
		{
			materialIndexToName[i] = System::MaterialSystem::s_DefaultPBRInstanceStr;
			continue;
		}

		materialIndexToName[i] = name;
		materialInstance->get().RawCopy((void*)&materialData, sizeof(materialData));
	}
	//

	fastgltf::iterateSceneNodes(gltf.LoadedAsset, 0, fastgltf::math::fmat4x4(),
		[&](const fastgltf::Node& node, fastgltf::math::fmat4x4 matrix) {

			flecs::entity entity = m_World.entity(node.name.c_str());
			std::visit(fastgltf::visitor{
				[](const auto& arg) {},
				[&](const fastgltf::TRS& tf) {
					Component::PositionComponent position(tf.translation.x(), tf.translation.y(), tf.translation.z());
					glm::quat rotQuat(tf.rotation.w(), tf.rotation.x(), tf.rotation.y(), tf.rotation.z()); // glm::quat takes w as the first param while fastgltf takes it as the last... makes no sense when glm stores w last... bruv
					glm::vec3 rotation = glm::degrees(glm::eulerAngles(rotQuat));
					Component::ScaleComponent scale(tf.scale.x(), tf.scale.y(), tf.scale.z());
					entity.set<Component::PositionComponent>(position);
					entity.set<Component::RotationComponent>(rotation);
					entity.set<Component::ScaleComponent>(scale);
					entity.add<Component::StaticComponent>();
				},
				[&](const fastgltf::math::fmat4x4& tf) {

				}
				}, node.transform);

			if (node.meshIndex.has_value()) {
				auto mesh = meshSystem.GetMesh(meshIndexToName[node.meshIndex.value()]);
				if (mesh) {
					Component::MeshComponent meshComponent(mesh.value().get(), defaultPbrInstance->get());
					for (int32_t i = 0; i < meshComponent.m_NumSubmeshes; i++)
					{
						if (auto iter = materialIndexToName.find(gltf.LoadedAsset.meshes[node.meshIndex.value()].primitives[i].materialIndex.value()); iter != materialIndexToName.end())
						{
							auto submeshMaterial = matSystem.GetMaterialInstance(iter->second);
							if (!submeshMaterial)
							{
								continue;
							}
							meshComponent.SetMaterial(i, submeshMaterial->get());
						}
					}
					entity.set<Component::MeshComponent>(meshComponent);
				}
			}
			//if (node.cameraIndex.has_value()) {
			//	const fastgltf::Camera& camera = asset->cameras[node.cameraIndex.value()];
			//	std::visit(fastgltf::visitor{
			//		[](const auto& arg) {},
			//		[&](const fastgltf::Camera::Perspective& perspective) {
			//			Component::CameraComponent cameraComponent(Component::CameraComponent::EProjectionType::Perspective, static_cast<float>(perspective.yfov),
			//				static_cast<float>(perspective.znear), perspective.zfar.has_value() ? static_cast<float>(perspective.zfar.value()) : 1000.f /* default to 1000.f */,
			//				//perspective.aspectRatio.has_value() ? static_cast<float>(perspective.aspectRatio.value()) : 16.f / 9.f /* default to 16:9 */,
			//				true, Helper::Rectangle(0,0,800,600)); // todo Handle loading with aspect ratio instead of hardcoding width and height
			//			entity.set<Component::CameraComponent>(cameraComponent);
			//		},
			//		[&](const fastgltf::Camera::Orthographic orthographic) {
			//			Component::CameraComponent cameraComponent(Component::Camera::EProjectionType::Ortographic, 0.f,
			//				static_cast<float>(orthographic.znear), static_cast<float>(orthographic.zfar),
			//				true, Helper::Rectangle(0,0,800,600)); // todo Handle loading using orthographic.xmag and orthographic.ymag
			//			entity.set<Component::CameraComponent>(cameraComponent);
			//		}
			//		}, camera.camera);
			//}
			//if (node.lightIndex.has_value()) {
			//	const fastgltf::Light& light = asset->lights[node.lightIndex.value()];
			//	switch (light.type)
			//	{
			//	case fastgltf::LightType::Point:
			//	{
			//		Component::PointLightComponent p;
			//		p.color = { light.color.x(), light.color.y(), light.color.z() };
			//		p.intensity = light.intensity;
			//		entity.set<Component::PointLight>(p);
			//		break;
			//	}
			//	case fastgltf::LightType::Spot:
			//	{
			//		Component::SpotLightComponent p;
			//		p.color = { light.color.x(), light.color.y(), light.color.z() };
			//		p.intensity = light.intensity;

			//		// todo Figure out which ones that should be used and how
			//		/*p.coneAngle = light.innerConeAngle.has_value() ? light.innerConeAngle.value() : 0.f;
			//		p.coneAngle = light.outerConeAngle.has_value() ? light.outerConeAngle.value() : 0.f;*/

			//		entity.set<Component::SpotLightComponent>(p);
			//		break;
			//	}
			//	case fastgltf::LightType::Directional:
			//	{
			//		Component::DirectionalLightComponent p;
			//		p.color = { light.color.x(), light.color.y(), light.color.z() };
			//		p.intensity = light.intensity;
			//		entity.set<Component::DirectionalLightComponent>(p);
			//		break;
			//	}
			//	default:
			//		break;
			//	}
			//}
		});
}

// todo If fail -> empty
bool aZero::Scene::Scene::LoadGltf(const std::filesystem::path& path, System::MeshSystem& meshSystem, System::MaterialSystem& matSystem, System::TextureSystem& tSystem)
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

	std::chrono::high_resolution_clock clock;

	auto start = clock.now();

	// ------------------------------------------------------------------------------------------------------------------------------------------------
	// LOAD TEXTURES
	// ------------------------------------------------------------------------------------------------------------------------------------------------
	std::cout << "------------------------------------\nTextures loading... ";

	std::unordered_map<uint32_t, std::string> textureIndexToName = this->LoadGltf_Textures(path, tSystem, asset);

	auto end = clock.now();
	std::chrono::duration<double> diff = end - start;
	std::cout << "Completed after: " << diff << "\n";

	// ------------------------------------------------------------------------------------------------------------------------------------------------
	// LOAD MESHES
	// ------------------------------------------------------------------------------------------------------------------------------------------------
	start = clock.now();

	std::cout << "------------------------------------\nMeshes loading... ";

	std::unordered_map<uint32_t, std::string> meshIndexToName = this->LoadGltf_Meshes(path, meshSystem, asset);

	end = clock.now();
	diff = end - start;
	std::cout << "Completed after: " << diff << "\n";

	// ------------------------------------------------------------------------------------------------------------------------------------------------
	// LOAD MATERIALS
	// ------------------------------------------------------------------------------------------------------------------------------------------------
	start = clock.now();

	std::cout << "------------------------------------\nMaterials loading... ";

	std::unordered_map<uint32_t, std::string> materialIndexToName = this->LoadGltf_Materials(path, matSystem, tSystem, asset, textureIndexToName);

	end = clock.now();
	diff = end - start;
	std::cout << "Completed after: " << diff << "\n";

	// ------------------------------------------------------------------------------------------------------------------------------------------------
	// LOAD NODES
	// ------------------------------------------------------------------------------------------------------------------------------------------------
	start = clock.now();

	std::cout << "------------------------------------\nScene nodes loading... ";

	auto defaultPbrInstance = matSystem.GetMaterialInstance(System::MaterialSystem::s_DefaultPBRInstanceStr);
	if (!defaultPbrInstance)
	{
		return false;
	}

	fastgltf::iterateSceneNodes(*asset, 0, fastgltf::math::fmat4x4(),
		[&](const fastgltf::Node& node, fastgltf::math::fmat4x4 matrix) {

			flecs::entity entity = m_World.entity(node.name.c_str());
			std::visit(fastgltf::visitor{
				[](const auto& arg) {},
				[&](const fastgltf::TRS& tf) {
					Component::PositionComponent position(tf.translation.x(), tf.translation.y(), tf.translation.z());
					glm::quat rotQuat(tf.rotation.w(), tf.rotation.x(), tf.rotation.y(), tf.rotation.z()); // glm::quat takes w as the first param while fastgltf takes it as the last... makes no sense when glm stores w last... bruv
					glm::vec3 rotation = glm::degrees(glm::eulerAngles(rotQuat));
					Component::ScaleComponent scale(tf.scale.x(), tf.scale.y(), tf.scale.z());
					entity.set<Component::PositionComponent>(position);
					entity.set<Component::RotationComponent>(rotation);
					entity.set<Component::ScaleComponent>(scale);
					entity.add<Component::StaticComponent>();
				},
				[&](const fastgltf::math::fmat4x4& tf) {

				}
				}, node.transform);

			if (node.meshIndex.has_value()) {
				auto mesh = meshSystem.GetMesh(meshIndexToName[node.meshIndex.value()]);
				if (mesh) {
					Component::MeshComponent meshComponent(mesh.value().get(), defaultPbrInstance->get());
					for (int32_t i = 0; i < meshComponent.m_NumSubmeshes; i++)
					{
						if (auto iter = materialIndexToName.find(asset->meshes[node.meshIndex.value()].primitives[i].materialIndex.value()); iter != materialIndexToName.end())
						{
							auto submeshMaterial = matSystem.GetMaterialInstance(iter->second);
							if (!submeshMaterial)
							{
								continue;
							}
							meshComponent.SetMaterial(i, submeshMaterial->get());
						}
					}
					entity.set<Component::MeshComponent>(meshComponent);
				}
			}
			//if (node.cameraIndex.has_value()) {
			//	const fastgltf::Camera& camera = asset->cameras[node.cameraIndex.value()];
			//	std::visit(fastgltf::visitor{
			//		[](const auto& arg) {},
			//		[&](const fastgltf::Camera::Perspective& perspective) {
			//			Component::CameraComponent cameraComponent(Component::CameraComponent::EProjectionType::Perspective, static_cast<float>(perspective.yfov),
			//				static_cast<float>(perspective.znear), perspective.zfar.has_value() ? static_cast<float>(perspective.zfar.value()) : 1000.f /* default to 1000.f */,
			//				//perspective.aspectRatio.has_value() ? static_cast<float>(perspective.aspectRatio.value()) : 16.f / 9.f /* default to 16:9 */,
			//				true, Helper::Rectangle(0,0,800,600)); // todo Handle loading with aspect ratio instead of hardcoding width and height
			//			entity.set<Component::CameraComponent>(cameraComponent);
			//		},
			//		[&](const fastgltf::Camera::Orthographic orthographic) {
			//			Component::CameraComponent cameraComponent(Component::Camera::EProjectionType::Ortographic, 0.f,
			//				static_cast<float>(orthographic.znear), static_cast<float>(orthographic.zfar),
			//				true, Helper::Rectangle(0,0,800,600)); // todo Handle loading using orthographic.xmag and orthographic.ymag
			//			entity.set<Component::CameraComponent>(cameraComponent);
			//		}
			//		}, camera.camera);
			//}
			//if (node.lightIndex.has_value()) {
			//	const fastgltf::Light& light = asset->lights[node.lightIndex.value()];
			//	switch (light.type)
			//	{
			//	case fastgltf::LightType::Point:
			//	{
			//		Component::PointLightComponent p;
			//		p.color = { light.color.x(), light.color.y(), light.color.z() };
			//		p.intensity = light.intensity;
			//		entity.set<Component::PointLight>(p);
			//		break;
			//	}
			//	case fastgltf::LightType::Spot:
			//	{
			//		Component::SpotLightComponent p;
			//		p.color = { light.color.x(), light.color.y(), light.color.z() };
			//		p.intensity = light.intensity;

			//		// todo Figure out which ones that should be used and how
			//		/*p.coneAngle = light.innerConeAngle.has_value() ? light.innerConeAngle.value() : 0.f;
			//		p.coneAngle = light.outerConeAngle.has_value() ? light.outerConeAngle.value() : 0.f;*/

			//		entity.set<Component::SpotLightComponent>(p);
			//		break;
			//	}
			//	case fastgltf::LightType::Directional:
			//	{
			//		Component::DirectionalLightComponent p;
			//		p.color = { light.color.x(), light.color.y(), light.color.z() };
			//		p.intensity = light.intensity;
			//		entity.set<Component::DirectionalLightComponent>(p);
			//		break;
			//	}
			//	default:
			//		break;
			//	}
			//}
		});

	end = clock.now();
	diff = end - start;
	std::cout << "Completed after: " << diff << "\n";

	return true;
}