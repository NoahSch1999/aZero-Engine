#pragma once
#include "asset/mesh/Primitives.hpp"

namespace aZero::GPUStruct
{
    struct CameraData
    {
        glm::mat4 ViewMatrix;
        glm::mat4 ViewProjectionMatrix;
        aZero::BoundingVolume::BoundingFrustum Frustum;
    };

    struct InstanceData
    {
        glm::mat4x4 WorldMatrix;
        VkDeviceAddress MeshletBDA;
        VkDeviceAddress VertexBDA;
        VkDeviceAddress MeshletBoundsBDA;
        VkDeviceAddress MaterialPtr;
        uint32_t MeshletCount;
    };

    struct InstanceCullData
    {
        aZero::BoundingVolume::BoundingSphere WorldSpaceBounds;
        uint32_t MeshletCount;
        uint32_t MaterialTypeIndex;
    };

    struct MeshDrawPushConstants
    {
        VkDeviceAddress CameraBufferPtr;
        VkDeviceAddress InstanceDataBufferPtr;
        VkDeviceAddress DrawMetadataBufferPtr;

        VkDeviceAddress PointLightsPtr;
        uint32_t NumPointLights;
        VkDeviceAddress SpotLightsPtr;
        uint32_t NumSpotLights;
        VkDeviceAddress DirectionalLightsPtr;
        uint32_t NumDirectionalLights;

        uint32_t RenderMode;
    };

    struct ComputeCullConstants
    {
        VkDeviceAddress IndirectArgumentBufferPtr;
        VkDeviceAddress IndirectArgumentCounterBufferPtr;
        VkDeviceAddress CameraBufferPtr;
        VkDeviceAddress InstanceCullDataBufferPtr;
        VkDeviceAddress DrawMetadataBufferPtr;
        uint32_t InstanceCount;
    };

    struct MeshTaskIndirectArgs
    {
        uint32_t GroupX;
        uint32_t GroupY;
        uint32_t GroupZ;
    };

    struct IndirectArgumentCounter
    {
        uint32_t Count;
    };

    struct DrawMetaData
    {
        uint32_t InstanceIndex;
        uint32_t MeshletCount;
    };

    struct PBRMaterial
    {
        // todo Use the albedo alpha channel for roughness and normal w channel for metallic
        uint32_t AlbedoTexture;
        uint32_t NormalMap;
        uint32_t RoughnessMetallic;
        uint32_t GlowMap;
        uint32_t TransparencyMap;
    };

    struct PointLight
    {
         glm::vec3 Color;
         glm::vec3 Position;
         float Range;
         float Falloff;
         float Intensity;
    };

    struct SpotLight
    {
         glm::vec3 Color;
         glm::vec3 Position;
         glm::vec3 Direction;
         float Intensity;
         float Radius;
    };

    struct DirectionalLight
    {
        glm::vec3 Color;
        glm::vec3 Direction;
        float Intensity;
    };
}