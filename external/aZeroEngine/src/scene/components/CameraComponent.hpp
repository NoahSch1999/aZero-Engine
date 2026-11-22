#pragma once
#include "renderer/RenderTarget.hpp"
#include "misc/BoundingVolume.hpp"

namespace aZero::Component
{
	enum class ECameraType { ORTOGRAPHIC, PERSPECTIVE, MAX };
	class CameraComponent
	{
	public:
		ECameraType m_Type = ECameraType::MAX;

		bool m_IsActive = true;

		// When rendering cameras are sorted so the higher the priority index the later it is rendered with.
		uint32_t m_PriorityIndex = 0;

		// In range from 0 < fov < M_PI - 0.001f
		float m_Fov = 0.f;

		// In the range from 0 <= value <= 1
		float m_Top = 0.f, m_Left = 0.f, m_Bottom = 0.f, m_Right = 0.f, m_Near = 0.f, m_Far = 0.f;

		std::optional<std::reference_wrapper<Renderer::RenderTarget>> m_OptColorTarget, m_OptDepthStencilTarget;
		bool m_ClearColorTargetOnRender = false;
		bool m_ClearDepthStencilTargetOnRender = false;

		std::tuple<VkViewport, VkRect2D> GetViewportAndScizzorRect() const
		{
			glm::uvec2 dims = m_OptColorTarget->get().GetDimensions();

			int32_t top = (int32_t)(dims.y * (float)(m_Top / 1.f));
			int32_t left = (int32_t)(dims.x * (float)(m_Left / 1.f));
			int32_t bottom = (int32_t)(dims.y * (float)(m_Bottom / 1.f));
			int32_t right = (int32_t)(dims.x * (float)(m_Right / 1.f));
			uint32_t width = (uint32_t)(right - left);
			uint32_t height = (uint32_t)(bottom - top);

			VkRect2D rect{
				.offset{.x = left, .y = top},
				.extent{.width = width, .height = height}
			};

			// todo Verify valid values
			// We inverse the height since vk uses -y up
			VkViewport vp{
				.x = (float)left,
				.y = (float)top,
				.width = (float)width,
				.height = (float)height,
				.minDepth = 0.f,
				.maxDepth = 1.f // todo These should maybe be a setting too
			};
			return { vp, rect };
		}

		bool IsValidForRendering() const
		{
			const bool hasValidTarget = m_OptColorTarget.has_value() && m_OptDepthStencilTarget.has_value(); // todo Make this to || when we want to allow depth-only rendering
			const bool hasValidRect = 
				(m_Top >= 0.f && m_Left >= 0.f && m_Near >= 0.f)
				&& (m_Top < m_Bottom && m_Left < m_Right && m_Near < m_Far);

			// If it's perspective, it only has a valid fov if it's 0 - 360 degrees. For ortographics fov isn't used, so it's always valid.
			const bool hasValidFov = m_Type == ECameraType::PERSPECTIVE ? m_Fov > 0.f && m_Fov < M_PI - 0.001f : true;

			return hasValidTarget && hasValidRect && hasValidFov;
		}

		aZero::BoundingVolume::BoundingFrustum
			CreateFrustra(
				const glm::vec3& worldPosition,
				const glm::vec3& worldEulerRotation) const
		{
			aZero::BoundingVolume::BoundingFrustum frustra{};
			// todo Impl
			return frustra;
		}

		glm::mat4x4 CreateViewMatrix(const glm::vec3& worldPosition, const glm::vec3& rotationRadians) const
		{
			// Create rotation-quat using euler-angles.
			// The quat impl uses the angles to create a vector that we rotate around and the degrees that we rotate around the vector.
			glm::quat rotation = glm::quat(rotationRadians);

			// The rotation quat can be used to apply the rotation when multiplying with a vector.
			glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
			glm::vec3 up = rotation * glm::vec3(0.0f, 1.0f, 0.0f);

			glm::vec3 target = worldPosition + forward;

			return glm::lookAt(
				worldPosition,
				target,
				up
			);
		}

		glm::mat4x4 CreateProjectionMatrix() const
		{
			const auto& [vp, sr] = this->GetViewportAndScizzorRect();
			auto mat = glm::perspective(m_Fov, (float)sr.extent.width / (float)sr.extent.height, m_Near, m_Far);
			mat[1][1] *= -1.0f;
			return mat;
		}

	private:
	};
}