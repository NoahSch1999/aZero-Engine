#pragma once
#include "MaterialInstance.hpp"

namespace aZero::Asset
{
    struct PBRMaterialData
    {
        uint32_t AlbedoTexture = std::numeric_limits<uint32_t>::max();
        uint32_t NormalMap = std::numeric_limits<uint32_t>::max();
        uint32_t RoughnessMetallic = std::numeric_limits<uint32_t>::max();
        uint32_t GlowMap = std::numeric_limits<uint32_t>::max();
        uint32_t TransparencyMap = std::numeric_limits<uint32_t>::max();
        float RoughnessFactor = 0.f;
        float MetallicFactor = 0.f;
    };
}