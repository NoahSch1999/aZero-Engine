#pragma once
#include "render_api/Image.hpp"
#include "misc/Helpers.hpp"

namespace aZero::Renderer
{
	enum class ERenderTargetType { COLOR, DEPTH_STENCIL, MAX };
	class RenderTarget
	{
		friend class Renderer;
	public:

		RenderTarget() = default;

		bool Resize(const glm::uvec2& dimensions)
		{
			bool resizeRes = m_Image.GetDesc().OptResourceRecycler.has_value() && m_diCore && m_Type != ERenderTargetType::MAX;
			if(resizeRes)
			{
				resizeRes = this->Init(m_Type, dimensions, *m_diCore, m_Image.GetDesc().OptResourceRecycler.value());
			}
			return resizeRes;
		}

		RenderTarget(RenderTarget&& other) noexcept
		{
			*this = std::move(other);
		}

		RenderTarget& operator=(RenderTarget&& other) noexcept
		{
			std::swap(m_diCore, other.m_diCore);
			std::swap(m_Image, other.m_Image);
			std::swap(m_View, other.m_View);
			std::swap(m_Type, other.m_Type);
			return *this;
		}

	private:
		bool Init(ERenderTargetType type, const glm::uvec2& dimensions, RenderAPI::Core& core, aZero::Renderer::ResourceRecycler& resourceRecycler)
		{
			RenderAPI::Image image;
			aZero::RenderAPI::ImageView view;
			const bool initRes = this->InitInternal(type, dimensions, core, resourceRecycler, image, view);
			if (initRes)
			{
				// The old image resources are recycled via RAII and then emptied after 3 frames
				m_Image = std::move(image);
				m_View = std::move(view);

				m_diCore = &core;
				m_Type = type;
			}
			return initRes;
		}

		bool InitInternal(ERenderTargetType type, const glm::uvec2& dimensions, RenderAPI::Core& core, aZero::Renderer::ResourceRecycler& resourceRecycler, RenderAPI::Image& image, aZero::RenderAPI::ImageView& view)
		{
			RenderAPI::Image::Desc ctDesc{
				.Format = VkFormat::VK_FORMAT_R8G8B8A8_SRGB,
				.Type = VkImageType::VK_IMAGE_TYPE_2D,
				.Usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				.Width = (uint32_t)dimensions.x,
				.Height = (uint32_t)dimensions.y,
				.Depth = 1,
				.MipCount = 1,
				.ArrayLayers = 1,
				.OptResourceRecycler = resourceRecycler
			};

			if (type == ERenderTargetType::COLOR)
			{
				ctDesc.Format = VK_FORMAT_R8G8B8A8_SRGB;
				ctDesc.Usage |= VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			}
			else if (type == ERenderTargetType::DEPTH_STENCIL)
			{
				ctDesc.Format = VK_FORMAT_D24_UNORM_S8_UINT;
				ctDesc.Usage |= VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			}
			else
			{
				return false;
			}

			bool initRes = image.Init(core, ctDesc);
			if (!initRes)
			{
				return false;
			}

			aZero::RenderAPI::ImageView::Desc dsvView{
				.Image = &image,
				.Type = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D
			};

			if (type == ERenderTargetType::COLOR)
			{
				dsvView.ImageAspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
			}
			else if (type == ERenderTargetType::DEPTH_STENCIL)
			{
				dsvView.ImageAspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT | VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT;
			}
			else
			{
				return false;
			}

			return view.Init(core, dsvView);
		}
		RenderAPI::Core* m_diCore = nullptr;
		RenderAPI::Image m_Image;
		aZero::RenderAPI::ImageView m_View;
		ERenderTargetType m_Type = ERenderTargetType::MAX;
	};
}