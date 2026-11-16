#pragma once
#include "misc/Helpers.hpp"

namespace aZero::Component
{
    struct PointLightComponent
    {
        glm::vec3 Color;
        float Range;
        float Falloff;
        float Intensity;
    };

    struct SpotLightComponent
    {
        glm::vec3 Color;
        float Intensity;
        float Radius;
    };

    struct DirectionalLightComponent
    {
        glm::vec3 Color;
        float Intensity;
    };
}