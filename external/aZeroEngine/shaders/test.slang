import core_shaders.IMaterial;
import core_shaders.RasterVertex;
import core_shaders.Lights;

struct VSInput {
    float3 Pos;
    float2 UV;
};

[[vk::binding(0, 0)]]
Sampler2D textures[];

struct PushConstants
{
    [[vk::offset(0)]]
    float3 Color;
    float4x4 VP;

    uint64_t MaterialPtr;
    PointLight* PointLightsPtr;
    uint32_t NumPointLights;

    SpotLight* SpotLightsPtr;
    uint32_t NumSpotLights;

    DirectionalLight* DirectionalLightsPtr;
    uint32_t NumDirectionalLights;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> Constants;

struct VSOutput {
    float4 Pos : SV_POSITION;
    float2 UV;
};

[shader("vertex")]
RasterVertex vsMain(VSInput input) {
    RasterVertex output;
    // output.Pos = mul(Constants.VP, float4(input.Pos.x, input.Pos.y * -1.0, input.Pos.z, 1.0));
    output.Position = float4(input.Pos, 1.f);
    output.UV = input.UV;
    output.MaterialIndex = 0;
    output.Meshletid = 0;
    output.Normal = float3(1, 2, 3);
    output.WorldPosition = float3(input.Pos);

    return output;
}

[shader("fragment")]
float4 psMain<T : IMaterial>(RasterVertex input) {
    float4 color = float4(0, 0, 0, 1);

    T material;
    color = material.CalcPointLights(color, Constants.MaterialPtr, input, Constants.PointLightsPtr, Constants.NumPointLights);
    color = material.CalcSpotLights(color, Constants.MaterialPtr, input, Constants.SpotLightsPtr, Constants.NumSpotLights);
    color = material.CalcDirectionalLights(color, Constants.MaterialPtr, input, Constants.DirectionalLightsPtr, Constants.NumDirectionalLights);

    return color;
}