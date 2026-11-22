#pragma once
#include "render_api/Image.hpp"
#include "misc/Helpers.hpp"
#include "misc/IndexFreelist.hpp"

namespace aZero::Renderer
{
	enum class ERenderTargetType { COLOR, DEPTH_STENCIL, MAX };
	class RenderTarget
	{
		friend class Renderer;
	public:

		RenderTarget() = default;
		~RenderTarget()
		{
			if (m_diIndexFreelist)
			{
				m_diIndexFreelist->Recycle(m_SampleDescriptor.Index);
			}
		}

		bool Resize(const glm::uvec2& dimensions)
		{
			bool resizeRes = m_Image.GetDesc().OptResourceRecycler.has_value() && m_diCore && m_Type != ERenderTargetType::MAX;
			if(resizeRes)
			{
				resizeRes = this->Init(m_Type, dimensions, *m_diCore, m_Image.GetDesc().OptResourceRecycler.value(), *m_diIndexFreelist, *m_diDescriptorSet);
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
			std::swap(m_SampleView, other.m_SampleView);
			std::swap(m_SampleDescriptor, other.m_SampleDescriptor);
			std::swap(m_diIndexFreelist, other.m_diIndexFreelist);
			std::swap(m_SampleSampler, other.m_SampleSampler);
			std::swap(m_diDescriptorSet, other.m_diDescriptorSet);
			return *this;
		}

		glm::uvec2 GetDimensions() const
		{
			glm::uvec2 dims;
			dims.x = m_Image.GetDesc().Width;
			dims.y = m_Image.GetDesc().Height;
			return dims;
		}

		RenderAPI::Image& GetImage() { return m_Image; }
		RenderAPI::ImageView& GetImageView() { return m_View; }

	private:
		bool Init(ERenderTargetType type, const glm::uvec2& dimensions, RenderAPI::Core& core, aZero::Renderer::ResourceRecycler& resourceRecycler, 
			aZero::DS::IndexFreelist<uint32_t>& diIndexFreelist, RenderAPI::DescriptorSet& diDescriptorSet)
		{
			RenderAPI::Image image;
			aZero::RenderAPI::ImageView view, sampledView;
			const bool initRes = this->InitInternal(type, dimensions, core, resourceRecycler, image, view, sampledView);
			if (initRes)
			{
				// The old image resources are recycled via RAII and then emptied after 3 frames
				m_Image = std::move(image);
				m_View = std::move(view);
				m_SampleView = std::move(sampledView);

				m_SampleSampler = RenderAPI::Sampler(core, {}); // todo Handle per-texture sampler

				m_SampleDescriptor = RenderAPI::DescriptorHandle{
					.Sampler = &m_SampleSampler,
					.View = &m_View,
					.Layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
				};

				if (m_SampleDescriptor.Index == RenderAPI::g_InvalidDescriptorIndex)
				{
					m_SampleDescriptor.Index = diIndexFreelist.New();
				}

				diDescriptorSet.UpdateDescriptor(m_SampleDescriptor);

				m_diCore = &core;
				m_Type = type;
				m_diIndexFreelist = &diIndexFreelist;
				m_diDescriptorSet = &diDescriptorSet;
			}
			return initRes;
		}

		bool InitInternal(ERenderTargetType type, const glm::uvec2& dimensions, 
			RenderAPI::Core& core, aZero::Renderer::ResourceRecycler& resourceRecycler, RenderAPI::Image& image, aZero::RenderAPI::ImageView& view, aZero::RenderAPI::ImageView& sampledView)
		{
			RenderAPI::Image::Desc ctDesc{
				.Format = VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT,
				.Type = VkImageType::VK_IMAGE_TYPE_2D,
				.Usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.Width = (uint32_t)dimensions.x,
				.Height = (uint32_t)dimensions.y,
				.Depth = 1,
				.MipCount = 1,
				.ArrayLayers = 1,
				.OptResourceRecycler = resourceRecycler
			};

			if (type == ERenderTargetType::COLOR)
			{
				ctDesc.Format = /*VkFormat::VK_FORMAT_R8G8B8A8_SRGB*/VK_FORMAT_R16G16B16A16_SFLOAT /*HDR format*/; // todo Test VK_FORMAT_B10G11R11_UFLOAT_PACK32
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

			aZero::RenderAPI::ImageView::Desc viewDesc{
				.Image = &image,
				.Type = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D
			};

			if (type == ERenderTargetType::COLOR)
			{
				viewDesc.ImageAspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
			}
			else if (type == ERenderTargetType::DEPTH_STENCIL)
			{
				viewDesc.ImageAspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;
			}
			else
			{
				return false;
			}

			if (!view.Init(core, viewDesc))
			{
				return false;
			}

			return true;
		}
		RenderAPI::Core* m_diCore = nullptr;
		aZero::DS::IndexFreelist<uint32_t>* m_diIndexFreelist = nullptr;
		RenderAPI::DescriptorSet* m_diDescriptorSet = nullptr;
		RenderAPI::Image m_Image;
		aZero::RenderAPI::ImageView m_View;

		RenderAPI::DescriptorHandle m_SampleDescriptor;
		aZero::RenderAPI::ImageView m_SampleView;
		RenderAPI::Sampler m_SampleSampler;
		ERenderTargetType m_Type = ERenderTargetType::MAX;
	};
}