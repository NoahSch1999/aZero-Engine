#ifndef TESTMATERIAL_DEFINITIONS_INCLUDED
#define TESTMATERIAL_DEFINITIONS_INCLUDED

import core_shaders.IMaterial;
import core_shaders.RasterVertex;
import core_shaders.Lights;

public struct TestMaterial : IMaterial
{
    [mutating] public void PreLightCalc(uint64_t materialPtr, RasterVertex vertex)
    {

    }

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
    float3 color;
    [mutating] public void PreLightCalc(uint64_t materialPtr, RasterVertex vertex)
    {
        let mat = (Data *)materialPtr;
        let tex = textures[mat->tex];
        color = tex.Sample(vertex.UV).rgb;
    }
    public float4 CalcPointLights(float4 fragmentInput, uint64_t materialPtr, RasterVertex vertex, PointLight *lights, uint32_t numLights)
    {
        float3 output = color;
        for (int i = 0; i < numLights; i++)
        {
             PointLight *l = &lights[i];
             float3 dir = l.Position - vertex.WorldPosition; // fragment wp to light dir
             output = output * l.Color * max(dot(dir, vertex.Normal), 0.f);
        }
        return float4(output, 1.f);
    }
    public float4 CalcSpotLights(float4 fragmentInput, uint64_t materialPtr, RasterVertex vertex, SpotLight *lights, uint32_t numLights)
    {
        return float4(fragmentInput);
    }
    public float4 CalcDirectionalLights(float4 fragmentInput, uint64_t materialPtr, RasterVertex vertex, DirectionalLight *lights, uint32_t numLights)
    {
        return float4(fragmentInput);
    }
}

#endif