#pragma once
#include "misc/Helpers.hpp"

namespace aZero::Component
{
	struct PositionComponent
	{
		glm::vec3 Position;
		PositionComponent() = default;
		PositionComponent(const glm::vec3& position)
			:Position(position) {
		}
		PositionComponent(float x, float y, float z)
			:Position(x, y, z){ }
	};

	// Rotations are in degrees, not radians.
	struct RotationComponent
	{
		glm::vec3 Rotation;
		RotationComponent() = default;
		RotationComponent(const glm::vec3& rotations)
			:Rotation(rotations) {
		}
		RotationComponent(float xAxis, float yAxis, float zAxis)
			:Rotation(xAxis, yAxis, zAxis) {
		}
	};

	struct ScaleComponent
	{
		glm::vec3 Scale;
		ScaleComponent() = default;
		ScaleComponent(const glm::vec3& scale)
			:Scale(scale) {
		}
		ScaleComponent(float x, float y, float z)
			:Scale(x, y, z) {
		}
		ScaleComponent(float uniformScale)
			:Scale(uniformScale, uniformScale, uniformScale) {
		}
	};
}