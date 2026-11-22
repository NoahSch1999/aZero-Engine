module IMaterial;

import Lights;
import RasterVertex;

public static const uint32_t gInvalidTexture = 0xFFFFFFFF;

[[vk::binding(0, 0)]]
public Sampler2D textures[];

public interface IMaterial
{
    [mutating] public void PreLightCalc(uint64_t materialPtr, RasterVertex vertex);
    public float4 CalcPointLights(float4 fragmentInput, uint64_t materialPtr, RasterVertex vertex, PointLight* lights, uint32_t numLights);
    public float4 CalcSpotLights(float4 fragmentInput, uint64_t materialPtr, RasterVertex vertex, SpotLight* lights, uint32_t numLights);
    public float4 CalcDirectionalLights(float4 fragmentInput, uint64_t materialPtr, RasterVertex vertex, DirectionalLight *lights, uint32_t numLights);
};