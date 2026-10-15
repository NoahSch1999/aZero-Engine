
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
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> Constants;

struct VSOutput {
    float4 Pos : SV_POSITION;
    float2 UV;
};

[shader("vertex")]
VSOutput vsMain(VSInput input) {
    VSOutput output;
    // output.Pos = mul(Constants.VP, float4(input.Pos.x, input.Pos.y * -1.0, input.Pos.z, 1.0));
    output.Pos = float4(input.Pos, 1.f);
    output.UV = input.UV;

    return output;
}

[shader("fragment")]
float4 psMain(VSOutput input) {
    return float4(Constants.Color, 1.f);
}