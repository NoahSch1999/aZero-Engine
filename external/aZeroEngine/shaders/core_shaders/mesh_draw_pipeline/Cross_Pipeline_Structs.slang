module Cross_Pipeline_Structs;

import Volumes;
import Util;
import Lights;

public struct CameraData
{
    public float4x4 ViewMatrix;
    public float4x4 ViewProjectionMatrix;
    public BoundingFrustum Frustum;
};

public struct SubmeshMetadata
{
    public BoundingSphere Bounds;
    public uint64_t VertexBufferPtr;
    public uint64_t MeshletBufferPtr;
    public uint64_t MeshletBoundsBufferPtr;
    public uint32_t MeshletCount;
};

public struct InstanceData
{
    public float4x4 WorldMatrix;
    public uint64_t MaterialPtr;
};

public struct InstanceCullData
{
    public uint32_t SubmeshMetadataIndex;
    public uint32_t MaterialTypeIndex;
};

public struct DrawPushConstants
{
    public uint64_t CameraBufferPtr;
    public uint64_t InstanceDataBufferPtr; // todo This need to point to the start of the materialtype instances
    public uint64_t DrawMetadataBufferPtr; // todo This need to point to the start of the materialtype instances
    public PointLight *PointLightsPtr;
    public uint32_t NumPointLights;
    public SpotLight *SpotLightsPtr;
    public uint32_t NumSpotLights;
    public DirectionalLight *DirectionalLightsPtr;
    public uint32_t NumDirectionalLights;
    public uint32_t RenderMode;
};

public struct ComputeCullConstants
{
    public uint64_t IndirectArgumentBufferPtr;
    public uint64_t IndirectArgumentCounterBufferPtr;
    public uint64_t CameraBufferPtr;
    public uint64_t InstanceDataBufferPtr;
    public uint64_t InstanceCullDataBufferPtr;
    public uint64_t DrawMetadataBufferPtr;
    public uint64_t SubmeshMetadataBufferPtr;
    public uint32_t InstanceCount;
};

public struct MeshTaskIndirectArgs
{
    public uint32_t GroupX;
    public uint32_t GroupY;
    public uint32_t GroupZ;
};

public struct DrawMetaData
{
    public uint32_t InstanceIndex;
    public uint32_t MeshletCount;
    public uint64_t VertexBufferPtr;
    public uint64_t MeshletBufferPtr;
    public uint64_t MeshletBoundsBufferPtr;
};

public static const uint g_ThreadCount = 32;