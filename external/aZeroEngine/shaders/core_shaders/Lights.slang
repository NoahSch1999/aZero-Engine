#ifndef LIGHTS_DEFINITIONS_INCLUDED
#define LIGHTS_DEFINITIONS_INCLUDED

module Lights;

public struct PointLight
{
    public float3 Color;
    public float3 Position;
    public float Range;
    public float Falloff;
    public float Intensity;
}

public struct SpotLight
{
    public float3 Color;
    public float3 Position;
    public float3 Direction;
    public float Intensity;
    public float Radius;
}

public struct DirectionalLight
{
    public float3 Color;
    public float3 Direction;
    public float Intensity;
}

#endif