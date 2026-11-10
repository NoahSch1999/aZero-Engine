#ifndef TESTMATERIAL_DEFINITIONS_INCLUDED
#define TESTMATERIAL_DEFINITIONS_INCLUDED

import core_shaders.IMaterial;
import core_shaders.RasterVertex;
import core_shaders.Lights;

public struct TestMaterial : IMaterial
{
    public float4 CalcPointLights(float4 fragmentInput, uint64_t materialPtr, RasterVertex vertex, PointLight *lights, uint32_t numLights)
    {
        return float4(1,0,0, 1.f);
    }
    public float4 CalcSpotLights(float4 fragmentInput, uint64_t materialPtr, RasterVertex vertex, SpotLight *lights, uint32_t numLights)
    {
        return float4(lights[0].Color, 1.f);
    }
    public float4 CalcDirectionalLights(float4 fragmentInput, uint64_t materialPtr, RasterVertex vertex, DirectionalLight *lights, uint32_t numLights)
    {
        return float4(1, 0, 0, 1.f);
    }
}

public struct TestMaterial2 : IMaterial
{
    struct Data
    {
        float1 x;
        int3 y;
        uint32_t tex;
    }
    public float4 CalcPointLights(float4 fragmentInput, uint64_t materialPtr, RasterVertex vertex, PointLight *lights, uint32_t numLights)
    {
        return float4(1, 1, 1, 1.f);
    }
    public float4 CalcSpotLights(float4 fragmentInput, uint64_t materialPtr, RasterVertex vertex, SpotLight *lights, uint32_t numLights)
    {
        return float4(1, 0, 1, 1.f);
    }
    public float4 CalcDirectionalLights(float4 fragmentInput, uint64_t materialPtr, RasterVertex vertex, DirectionalLight *lights, uint32_t numLights)
    {
        Data *data = (Data *)materialPtr;
        let tex = textures[data->tex];
        return float4(tex.Sample(vertex.UV).rgb, 1.f);
    }
}

#endif