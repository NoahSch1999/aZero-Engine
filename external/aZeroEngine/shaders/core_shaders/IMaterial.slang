#ifndef IMATERIAL_DEFINITIONS_INCLUDED
#define IMATERIAL_DEFINITIONS_INCLUDED

module IMaterial;

import Lights;
import RasterVertex;

public interface IMaterial
{
    public float4 CalcPointLights(float4 fragmentInput, uint64_t materialPtr, RasterVertex vertex, PointLight* lights, uint32_t numLights);
    public float4 CalcSpotLights(float4 fragmentInput, uint64_t materialPtr, RasterVertex vertex, SpotLight* lights, uint32_t numLights);
    public float4 CalcDirectionalLights(float4 fragmentInput, uint64_t materialPtr, RasterVertex vertex, DirectionalLight *lights, uint32_t numLights);
};

#endif