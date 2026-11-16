#pragma once
#include "misc/Helpers.hpp"

namespace aZero::Component
{
    struct RigidbodyComponent : public Physics::TriggerBody
    {
        RigidbodyComponent() = default;

        RigidbodyComponent(const JPH::BodyCreationSettings& settings)
            :Physics::TriggerBody(settings) {}

        RigidbodyComponent& operator=(const JPH::BodyCreationSettings& settings)
        {
            Physics::TriggerBody::operator=(settings);
            return *this;
        }
    };
}