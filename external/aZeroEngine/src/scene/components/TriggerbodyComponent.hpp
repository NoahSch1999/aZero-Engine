#pragma once
#include "misc/Helpers.hpp"
#include "physics/TriggerBody.hpp"

namespace aZero::Component
{
    // todo Make it support multiple colliders in one component
    struct TriggerbodyComponent : public Physics::TriggerBody
    {
        TriggerbodyComponent() = default;

        TriggerbodyComponent(const JPH::BodyCreationSettings& settings)
            :Physics::TriggerBody(settings)
        {
            m_BodySettings.mIsSensor = true;
        }

        TriggerbodyComponent& operator=(const JPH::BodyCreationSettings& settings)
        {
            Physics::TriggerBody::operator=(settings);
            m_BodySettings.mIsSensor = true;
            return *this;
        }
    };
}