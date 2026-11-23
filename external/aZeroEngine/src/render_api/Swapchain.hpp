#pragma once
#include <array>
#include "Core.hpp"
#include "Image.hpp"
#include "Semaphore.hpp"
#include "CommandPool.hpp"
#include "CommandQueue.hpp"

namespace aZero::Renderer { class Renderer; }

namespace aZero::RenderAPI
{
    static inline bool chkSwapchain(VkResult result) {
        if (result < VK_SUCCESS) {
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                return false;
            }
            std::cerr << "Vulkan call returned an error (" << result << ")\n";
            return false;
        }
        return true;
    }

	class Swapchain : public DeleteCopy
	{
        friend class aZero::Renderer::Renderer;
	public:
        struct Desc
        {
            VkSurfaceKHR Surface = nullptr;
            VkFormat Format = VkFormat::VK_FORMAT_UNDEFINED;
            uint32_t FramesInFlight = 1;
            uint32_t Width = 0, Height = 0;
            VkPresentModeKHR PresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        };

        Swapchain() = default;
        ~Swapchain()
        {
            if (m_diCore)
            {
                vkDestroySwapchainKHR(m_diCore->GetLogicalDevice(), m_Swapchain, nullptr);
                vkDestroySurfaceKHR(m_diCore->GetInstance(), m_Surface, nullptr);
                for (auto& bbImageView : m_BackBufferViews)
                {
                    vkDestroyImageView(m_diCore->GetLogicalDevice(), bbImageView.Get(), nullptr);
                }
            }
        }

        Swapchain(Swapchain&& other) noexcept
        {
            *this = std::move(other);
        }

        Swapchain& operator=(Swapchain&& other) noexcept
        {
            std::swap(m_diCore, other.m_diCore);
            std::swap(m_CurrentFrameIndex, other.m_CurrentFrameIndex);
            std::swap(m_CurrentBackBuffer, other.m_CurrentBackBuffer);
            std::swap(m_Surface, other.m_Surface);
            std::swap(m_Swapchain, other.m_Swapchain);
            std::swap(m_CreateInfo, other.m_CreateInfo);
            std::swap(m_BackBuffers, other.m_BackBuffers);
            std::swap(m_BackBufferViews, other.m_BackBufferViews);
            std::swap(m_ImageReadySemaphores, other.m_ImageReadySemaphores);
            std::swap(m_RenderCompleteSemaphores, other.m_RenderCompleteSemaphores);
            return *this;
        }

        Swapchain(Core& core, const Desc& desc)
        {
            this->Init(core, desc);
        }

		bool Init(Core& core, const Desc& desc)
		{
            // get surface properties
            VkSurfaceCapabilitiesKHR surfaceCaps{};
            if (!chkSucceed(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(core.GetPhysicalDevice(), desc.Surface, &surfaceCaps)))
            {
                return false;
            }

            // get surface extents. On "Wayland" 0xFFFFFFFF means that we need to choose the dimensions
            VkExtent2D swapchainExtent{ surfaceCaps.currentExtent };
            if (surfaceCaps.currentExtent.width == 0xFFFFFFFF) {
                swapchainExtent = { .width = desc.Width, .height = desc.Height };
            }

            VkExtent2D extents{ swapchainExtent };
            VkSwapchainCreateInfoKHR swapchainCI{
                .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                .surface = desc.Surface,
                .minImageCount = std::min(desc.FramesInFlight, surfaceCaps.maxImageCount), // If desc inputs more than supported, we set max as count
                .imageFormat = desc.Format,
                .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
                .imageExtent = extents,
                .imageArrayLayers = 1,
                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
                .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                .presentMode = desc.PresentMode
            };

            if(!chkSucceed(vkCreateSwapchainKHR(core.GetLogicalDevice(), &swapchainCI, nullptr, &m_Swapchain)))
            {
                return false;
            }

            uint32_t imageCount{ 0 };
            if(!chkSucceed(vkGetSwapchainImagesKHR(core.GetLogicalDevice(), m_Swapchain, &imageCount, nullptr)))
            {
                return false;
            }

            std::vector<VkImage> images(imageCount);
            if(!chkSucceed(vkGetSwapchainImagesKHR(core.GetLogicalDevice(), m_Swapchain, &imageCount, images.data())))
            {
                return false;
            }

            m_BackBuffers.reserve(imageCount);
            for (auto& i : images)
            {
                RenderAPI::Image::Desc iDesc{
                    .Format = swapchainCI.imageFormat,
                    .Type = VkImageType::VK_IMAGE_TYPE_2D,
                    .Usage = swapchainCI.imageUsage,
                    .Width = swapchainCI.imageExtent.width,
                    .Height = swapchainCI.imageExtent.height,
                    .Depth = 1,
                    .MipCount = 1,
                    .ArrayLayers = 1
                };
                m_BackBuffers.emplace_back(core, iDesc, i);

                RenderAPI::Image::TransitionDesc t{
                    .DstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    .DstAccessMask = VK_ACCESS_NONE,
                    .NewLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .SubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
                };
                m_BackBuffers.back().Transition(t);
            }

            m_BackBufferViews.resize(imageCount);
            m_ImageReadySemaphores.resize(imageCount);
            m_RenderCompleteSemaphores.resize(imageCount);
            for (auto i = 0; i < imageCount; i++) {
                RenderAPI::ImageView::Desc vDesc{
                    .Image = &m_BackBuffers[i],
                    .Type = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D,
                    .ImageAspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
                    .LayerStart = 0,
                    .LayerCount = 1,
                    .MipStart = 0,
                    .MipCount = 1
                };

                if(!m_BackBufferViews[i].Init(core, vDesc))
                {
                    return false;
                }
                m_ImageReadySemaphores[i] = RenderAPI::Semaphore(core, RenderAPI::ESemaphoreType::Binary);
                m_RenderCompleteSemaphores[i] = RenderAPI::Semaphore(core, RenderAPI::ESemaphoreType::Binary);

                if (core.IsDebugNamesSupported())
                {

                    VkDebugUtilsObjectNameInfoEXT nameInfo{};
                    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
                    nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
                    nameInfo.objectHandle = (uint64_t)m_BackBuffers[i].Get();

                    std::string debugName("Swapchain BB: ");
                    debugName += std::to_string(i);
                    nameInfo.pObjectName = debugName.c_str();
                    vkSetDebugUtilsObjectNameEXT(core.GetLogicalDevice(), &nameInfo);
                }
            }

            m_Surface = desc.Surface;
            m_CreateInfo = swapchainCI;
            m_diCore = &core;
            return true;
		}

        void Resize(uint32_t width, uint32_t height)
        {
            m_diCore->FlushGPU();

            VkSurfaceCapabilitiesKHR caps{};
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                m_diCore->GetPhysicalDevice(),
                m_Surface,
                &caps
            );

            if (caps.currentExtent.width != UINT32_MAX) // When this isn't UINT32_MAX then that means that the system has chosen the extents itself. Ex: When resizing the window.
            {
                m_CreateInfo.imageExtent = caps.currentExtent;
            }
            else
            {
                m_CreateInfo.imageExtent.width =
                    std::clamp(width,
                        caps.minImageExtent.width,
                        caps.maxImageExtent.width);

                m_CreateInfo.imageExtent.height =
                    std::clamp(height,
                        caps.minImageExtent.height,
                        caps.maxImageExtent.height);
            }

            m_CreateInfo.oldSwapchain = m_Swapchain;

            chkSucceed(vkCreateSwapchainKHR(m_diCore->GetLogicalDevice(), &m_CreateInfo, nullptr, &m_Swapchain));
            m_BackBufferViews.clear();

            uint32_t newBackBufferCount;
            chkSucceed(vkGetSwapchainImagesKHR(m_diCore->GetLogicalDevice(), m_Swapchain, &newBackBufferCount, nullptr));

            std::vector<VkImage> images(newBackBufferCount);
            chkSucceed(vkGetSwapchainImagesKHR(m_diCore->GetLogicalDevice(), m_Swapchain, &newBackBufferCount, images.data()));

            m_BackBuffers.resize(newBackBufferCount);
            m_BackBufferViews.resize(newBackBufferCount);
            for (uint32_t i = 0; i < newBackBufferCount; i++)
            {
                RenderAPI::Image::Desc iDesc{
                    .Format = m_CreateInfo.imageFormat,
                    .Type = VkImageType::VK_IMAGE_TYPE_2D,
                    .Usage = m_CreateInfo.imageUsage,
                    .Width = m_CreateInfo.imageExtent.width,
                    .Height = m_CreateInfo.imageExtent.height,
                    .Depth = 1,
                    .MipCount = 1,
                    .ArrayLayers = 1
                };
                m_BackBuffers[i].Init(*m_diCore, iDesc, images[i]);

                RenderAPI::ImageView::Desc vDesc{
                        .Image = &m_BackBuffers[i],
                        .Type = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D,
                        .ImageAspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
                        .LayerStart = 0,
                        .LayerCount = 1,
                        .MipStart = 0,
                        .MipCount = 1
                };

                m_BackBufferViews[i].Init(*m_diCore, vDesc);
            }

            m_RenderCompleteSemaphores.resize(m_BackBuffers.size());
            for (auto& semaphore : m_RenderCompleteSemaphores) {
                semaphore = RenderAPI::Semaphore(*m_diCore, RenderAPI::ESemaphoreType::Binary);
            }
            vkDestroySwapchainKHR(m_diCore->GetLogicalDevice(), m_CreateInfo.oldSwapchain, nullptr);

            m_CurrentFrameIndex = 0;
        }

        uint32_t GetBackbufferCount() const { return m_BackBuffers.size(); }
        RenderAPI::Image& GetCurrentBackBuffer() { return m_BackBuffers.at(m_CurrentBackBuffer); }
        RenderAPI::ImageView& GetCurrentBackBufferView() { return m_BackBufferViews.at(m_CurrentBackBuffer); }
        RenderAPI::Semaphore* GetImageReadySemaphore() { return &m_ImageReadySemaphores.at(m_CurrentFrameIndex); }
        RenderAPI::Semaphore* GetRenderCompletedSemaphore() { return &m_RenderCompleteSemaphores.at(m_CurrentBackBuffer); }
        const VkSwapchainCreateInfoKHR& GetCreateInfo() const { return m_CreateInfo; }
	private:
        bool AcquireNextImage()
        {
            // This might not give us imageIndex that are in consecutive order, so we have a seperate index for:
            //      - The current back-buffer in the 0-1-2-n-0-1-2-n order
            //      - The semaphore signalled when it's available to use for any command queue commands
            return chkSwapchain(vkAcquireNextImageKHR(m_diCore->GetLogicalDevice(), m_Swapchain, UINT64_MAX, m_ImageReadySemaphores.at(m_CurrentFrameIndex).Get(), VK_NULL_HANDLE, &m_CurrentBackBuffer));
        }

        bool Present(RenderAPI::CommandQueue& queue)
        {
            std::array<VkSemaphore, 1> waitOnSemaphores{ this->GetRenderCompletedSemaphore()->Get() };
            VkPresentInfoKHR presentInfo{
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = (uint32_t)waitOnSemaphores.size(),
                .pWaitSemaphores = waitOnSemaphores.data(), // here we wait on the semaphore that is signalled when the command buffer has passed the OM
                .swapchainCount = 1u,
                .pSwapchains = &m_Swapchain,
                .pImageIndices = &m_CurrentBackBuffer
            };
            m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % m_BackBuffers.size();

            if (!chkSwapchain(vkQueuePresentKHR(queue.Get(), &presentInfo)))
            {
                return false;
            }
            return true;
        }

        /// <summary>
        /// Incremented after a new back-backbuffer has been queued for present.
        /// This is used to select a semaphore that in turn is used for signalling when the next back-backbuffer is acquired.
        /// Used to wait so that the back-backbuffer isnt operated on until it's acquired.
        /// </summary>
        uint32_t m_CurrentFrameIndex = 0;

        /// <summary>
        /// The actual back-backbuffer index that is currently being acquired in the 0-1-2-n-0-1-2-n queue.
        /// This represents the actual currently acquired back-buffer that will be used for the next present.
        /// </summary>
        uint32_t m_CurrentBackBuffer = 0;

        VkSurfaceKHR m_Surface;
		VkSwapchainKHR m_Swapchain;
        VkSwapchainCreateInfoKHR m_CreateInfo;
        std::vector<RenderAPI::Image> m_BackBuffers;
        std::vector<RenderAPI::ImageView> m_BackBufferViews;

        /// <summary>
        /// Signalled when the back-buffer is acquired.
        /// Waited on before doing any commands on the back-buffer.
        /// </summary>
        std::vector<RenderAPI::Semaphore> m_ImageReadySemaphores;

        /// <summary>
        /// Signalled when the back-buffer has had commands completed for it.
        /// Waited on when trying to present the back-buffer.
        /// </summary>
        std::vector<RenderAPI::Semaphore> m_RenderCompleteSemaphores;
        RenderAPI::Core* m_diCore = nullptr;
	};
}