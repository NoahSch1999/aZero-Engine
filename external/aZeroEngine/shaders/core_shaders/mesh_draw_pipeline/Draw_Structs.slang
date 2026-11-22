module DrawStructs;

import Cross_Pipeline_Structs;
import RasterVertex;

public struct Payload
{
    public uint MeshletIndex[32];
    public uint InstanceIndex;
    public uint MeshletCount;
    public uint64_t VertexBufferPtr;
    public uint64_t MeshletBufferPtr;
};

// MESH ASSET STRUCTS

public static const uint g_VerticesPerMeshlet = 64;
public static const uint g_PrimitivesPerMeshlet = 84;

public struct MeshVertex
{
    public float3 Position;
    public uint32_t UV;
    public uint32_t Normal;
};

public struct Meshlet
{
    public uint32_t VertexOffset;
    public uint32_t VertexCount;
    public uint32_t PrimitiveCount;
    public uint32_t Primitives[g_PrimitivesPerMeshlet];
};
//

public static const uint RENDER_MODE_LIT = 0;
public static const uint RENDER_MODE_UNLIT = 1;
public static const uint RENDER_MODE_NORMAL_MAP = 2;
public static const uint RENDER_MODE_GEOMETRY_NORMALS = 3;
public static const uint RENDER_MODE_NORMAL_MAP_NORMALS = 4;
public static const uint RENDER_MODE_MESHLET_IDS = 5;
public static const uint RENDER_MODE_UVS = 6;
public static const uint RENDER_MODE_MIP_LEVEL = 7;

[[vk::push_constant]]
public ConstantBuffer<DrawPushConstants> Constants;

[[vk::binding(0, 0)]] // [vk::binding(binding : int, set : int)]
public Sampler2D Textures[];