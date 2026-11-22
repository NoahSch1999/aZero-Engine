import core_shaders.IMaterial;
import core_shaders.RasterVertex;
import core_shaders.Lights;
import core_shaders.Util;

float4 encodeMask(uint32_t mask)
{
    return float4(
        float((mask >> 0u) & 0xFFu),
        float((mask >> 8u) & 0xFFu),
        float((mask >> 16u) & 0xFFu),
                float((mask >> 24u) & 0xFFu)
    ) / 255.0;
}

#define PI 3.1415

float DistributionGGX(float3 N, float3 H, float a)
{
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float k)
{
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float k)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, k);
    float ggx2 = GeometrySchlickGGX(NdotL, k);

    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float3 CalculatePBRWithoutLightRadiance(
    float3 N,
    float3 V,
    float3 L,
    float3 albedo,
    float metallic,
    float roughness,
    float3 lightColor,
    float lightIntensity)
{
    // Halfway vector
    float3 H = normalize(V + L);

    // Dot products
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    // Prevent numerical issues
    roughness = max(roughness, 0.045);

    // Roughness parameters
    float a = roughness * roughness;

    // Geometry remapping (direct lighting)
    float k = (roughness + 1.0);
    k = (k * k) / 8.0;

    // F0: dielectric = 0.04, metallic = albedo
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    // Cook-Torrance BRDF components
    float D = DistributionGGX(N, H, a);
    float G = GeometrySmith(N, V, L, k);
    float3 F = FresnelSchlick(HdotV, F0);

    // Specular BRDF
    float3 numerator = D * G * F;

    float denominator = 4.0 * NdotV * NdotL + 0.0001;

    float3 specular = numerator / denominator;

    // Energy conservation
    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);

    // Diffuse Lambertian BRDF
    float3 diffuse = kD * albedo / PI;

    return (diffuse + specular) * NdotL;
}

public struct PBRMaterial : IMaterial
{
    struct PBRData
    {
        // todo Use the albedo alpha channel for roughness and normal w channel for metallic
        uint32_t AlbedoTexture;
        uint32_t NormalMap;
        uint32_t RoughnessMetallic;
        uint32_t GlowMap;
        uint32_t TransparencyMap;
        float RoughnessFactor;
        float MetallicFactor;
    };
    float3 AlbedoColor;
    float3 Normal;
    float Roughness;
    float Metallic;

    [mutating] public void PreLightCalc(uint64_t materialPtr, RasterVertex vertex)
    {
        if (((PBRData *)materialPtr)->AlbedoTexture != gInvalidTexture)
        { 
            let tex = textures[((PBRData *)materialPtr)->AlbedoTexture];
            AlbedoColor = tex.Sample(vertex.UV).rgb;
        }
        else
        {
            AlbedoColor = float3(1, 0, 1);
        }

        if (((PBRData *)materialPtr)->NormalMap != gInvalidTexture)
        {
            let tex = textures[((PBRData *)materialPtr)->NormalMap];
            Normal = tex.Sample(vertex.UV).rgb;

            float3 tangent;
            float3 bitangent;
            float3 normal = normalize(vertex.Normal);
            CalcTangentAndBitangent(normal, tangent, bitangent);
            float3x3 TBN = float3x3(tangent, bitangent, normal);
            Normal = normalize(Normal * 2.f - 1.f);
            Normal = normalize(mul(Normal, TBN));
        }
        else
        {
            Normal = normalize(vertex.Normal);
        }

        if (((PBRData *)materialPtr)->RoughnessMetallic != gInvalidTexture)
        {
            let tex = textures[((PBRData *)materialPtr)->RoughnessMetallic];
            float3 val = tex.Sample(vertex.UV).rgb;
            Roughness = val.g;
            Metallic = val.b;
        }
        else
        {
            Roughness = 0.f;
            Metallic = 0.f;
        }
    }

    public float4 CalcPointLights(float4 fragmentInput, uint64_t materialPtr, RasterVertex vertex, PointLight *lights, uint32_t numLights)
    {
        float3 output = AlbedoColor * float3(0.2, 0.2, 0.2);
        //for (uint32_t i = 0; i < numLights; i++)
        //{
        //    PointLight p = lights[i];
        //    float3 lightVector = p.Position - vertex.WorldPosition;
        //    float distance = length(lightVector);
        //    float3 L = lightVector / max(distance, 0.0001);

        //    float3 V = normalize(float3(0, 0, 2) - vertex.WorldPosition);
        //    float3 effect = CalculatePBRWithoutLightRadiance(Normal, V, L, AlbedoColor, Metallic, Roughness, p.Color, p.Intensity);

        //    float attenuation = 1.0 / max(distance * distance, 0.01);

        //    float radiusFade = saturate(1.0 - distance / p.Range);
        //    attenuation *= radiusFade * radiusFade;
        //    float3 radiance = p.Color * p.Intensity * attenuation;
        //    output += effect * radiance;
        //}

        return float4(output, 1.f);
    }
    public float4 CalcSpotLights(float4 fragmentInput, uint64_t materialPtr, RasterVertex vertex, SpotLight *lights, uint32_t numLights)
    {
        return float4(fragmentInput);
    }
    public float4 CalcDirectionalLights(float4 fragmentInput, uint64_t materialPtr, RasterVertex vertex, DirectionalLight *lights, uint32_t numLights)
    {
        return fragmentInput;
    }
}


//PBRMaterial *materialBuffer = (PBRMaterial *)Constants.MaterialBufferPtr;

//PBRMaterial material = materialBuffer[pin.MaterialIndex];

//if (material.AlbedoTexture == 0xffffffff)
//{
//    output.color = float4(1, 0, 1, 1);
//    return output;
//}

//float3 sampledAlbedo = Textures[material.AlbedoTexture].Sample(pin.UV).xyz;

//float3 fragmentNormal;
//if (material.NormalMap == 0xffffffff)
//{
//    fragmentNormal = normalize(pin.Normal);
//}
//else
//{
//    const float3 sampledNormal = Textures[material.NormalMap].Sample(pin.UV).xyz;
//    float3 tangent;
//    float3 bitangent;
//    float3 normal = normalize(pin.Normal);
//    CalcTangentAndBitangent(normal, tangent, bitangent);
//    float3x3 TBN = float3x3(tangent, bitangent, normal);
//    fragmentNormal = normalize(sampledNormal * 2.f - 1.f);
//    fragmentNormal = normalize(mul(fragmentNormal, TBN));
//}

//if (Constants.RenderMode != RENDER_MODE_LIT)
//{
//    // -------------------------------   DEBUG ALBEDO   -------------------------------------------
//    if (Constants.RenderMode == RENDER_MODE_UNLIT)
//    {
//        output.color = float4(sampledAlbedo, 1);
//        return output;
//    }
//    // --------------------------------------------------------------------------------------------

//    if (material.NormalMap != 0xffffffff)
//    {
//        // -------------------------------   DEBUG NORMAL MAP   -----------------------------------
//        if (Constants.RenderMode == RENDER_MODE_NORMAL_MAP)
//        {
//            const float3 sampledNormal = Textures[material.NormalMap].Sample(pin.UV).xyz;
//            output.color = float4(sampledNormal * 0.5 + 0.5, 1);
//            return output;
//        }
//        // ----------------------------------------------------------------------------------------

//        // -------------------------------   DEBUG NORMALS WITH MAPS   ----------------------------
//        if (Constants.RenderMode == RENDER_MODE_NORMAL_MAP_NORMALS)
//        {
//            output.color = float4(fragmentNormal * 0.5 + 0.5, 1);
//            return output;
//        }
//        // ----------------------------------------------------------------------------------------
//    }

//    // -------------------------------   DEBUG UV   -----------------------------------------------
//    if (Constants.RenderMode == RENDER_MODE_UVS)
//    {
//        output.color = float4(pin.UV, 0, 1);
//        return output;
//    }
//    // --------------------------------------------------------------------------------------------

//    // -------------------------------   DEBUG NORMALS  -------------------------------------------
//    if (Constants.RenderMode == RENDER_MODE_GEOMETRY_NORMALS)
//    {
//        output.color = float4(pin.Normal * 0.5 + 0.5, 1);
//        return output;
//    }
//    // --------------------------------------------------------------------------------------------

//    // -------------------------------   DEBUG MESHLETS   -----------------------------------------
//    if (Constants.RenderMode == RENDER_MODE_MESHLET_IDS)
//    {
//        output.color = float4(pin.Meshletid, 1);
//        return output;
//    }
//    // --------------------------------------------------------------------------------------------

//    // -------------------------------   DEBUG MIP LEVEL   ----------------------------------------
//    if (Constants.RenderMode == RENDER_MODE_MIP_LEVEL)
//    {
//        float lod = Textures[material.AlbedoTexture].CalculateLevelOfDetail(pin.UV);
//        float t = saturate(lod / 13.f);
//        output.color = float4(1.f - t, 1.f - t, 1.f - t, 1.f);
//        return output;
//    }
//    // --------------------------------------------------------------------------------------------
//}