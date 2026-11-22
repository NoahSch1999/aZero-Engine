#ifndef LIGHTS_DEFINITIONS_INCLUDED
#define LIGHTS_DEFINITIONS_INCLUDED

module Lights;

public struct PointLight
{
    public float3 Color;
    public float3 Position;
    public float Range;
    public float Intensity;

    public float3 CalculateLightContribution(float3 worldPosition, float3 normal)
    {
        float3 toLight = Position - worldPosition;

        float distanceToLight = length(toLight);
        float3 lightDir = normalize(toLight);

        // Inverse-square attenuation
        float attenuation = 1.0 / max(
            distanceToLight * distanceToLight,
            0.0001
        );

        // Optional GLTF range cutoff
        if (Range > 0.0 && distanceToLight > Range)
            return float3(0.0);

        float NdotL = max(dot(normal, lightDir), 0.0);

        return Color * Intensity * attenuation * NdotL;
    }
}

public struct SpotLight
{
    public float3 Color;
    public float3 Position;
    public float3 Direction;
    public float Intensity;
    public float InnerConeAngle;
    public float OuterConeAngle;
}

public struct DirectionalLight
{
    public float3 Color;
    public float3 Direction;
    public float Intensity;
}

#endif