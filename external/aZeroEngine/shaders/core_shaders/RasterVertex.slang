#ifndef RASTERVERTEX_DEFINITIONS_INCLUDED
#define RASTERVERTEX_DEFINITIONS_INCLUDED

module RasterVertex;

public struct RasterVertex
{
    public float4 Position : SV_Position;
    public float3 WorldPosition : WORLD_POSITION;
    public float2 UV : UV;
    public float3 Normal : NORMAL;
    public float3 Meshletid : MESHLET_ID;
    public nointerpolation uint32_t MaterialIndex : MATERIAL_INDEX;
};

#endif