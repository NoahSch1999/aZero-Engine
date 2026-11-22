struct FullscreenQuadVertex
{
    float4 Position;
    float2 UV;
};

struct PushConstants
{
    uint32_t HDR_Texture_Index;
    float Exposure;
    uint32_t TonemapType;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> Constants;

[[vk::binding(0, 0)]] // [vk::binding(binding : int, set : int)]
Sampler2D Textures[];

struct VSOutput {
    float4 Position : SV_POSITION;
    float2 UV : UV;
};

float3 ACES_Tonemap(float3 color, float exposure)
{
    float3 tempColor = exposure * color;
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;

    return saturate(
        (tempColor * (a * tempColor + b)) /
        (tempColor * (c * tempColor + d) + e)
    );
}

float3 Reinhard_Tonemap(float3 color, float exposure)
{
    return float3(1.f) - exp(-color * Constants.Exposure);
}

enum ETonemapType
{
    NONE = 0,
    REINHARD = 1,
    ACES = 2
}

[shader("vertex")]
VSOutput vsMain(FullscreenQuadVertex input) {
    VSOutput output;
    output.Position = input.Position;
    output.UV = input.UV;
    return output;
}

[shader("fragment")]
float4 fsMain(VSOutput input)
{
    let tex = Textures[Constants.HDR_Texture_Index];
    float3 hdrColor = tex.Sample(input.UV).rgb;
    float3 ldrColor = hdrColor;
    if (Constants.TonemapType == ETonemapType::REINHARD)
    {
        ldrColor = Reinhard_Tonemap(hdrColor, Constants.Exposure);
    }
    else if (Constants.TonemapType == ETonemapType::ACES)
    {
        ldrColor = ACES_Tonemap(hdrColor, Constants.Exposure);
    }
    return float4(ldrColor, 1.f);
}