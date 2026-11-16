#pragma once
#include "MaterialInstance.hpp"

namespace aZero::Asset
{
    struct PBRMaterialData
    {
        uint32_t AlbedoTexture;
        uint32_t NormalMap;
        uint32_t RoughnessMetallic;
        uint32_t GlowMap;
        uint32_t TransparencyMap;
        float RoughnessFactor;
        float MetallicFactor;
    };
}