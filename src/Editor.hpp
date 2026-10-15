#pragma once
#include "src/RenderWindow.hpp"
#include <SDL3/SDL_vulkan.h>

namespace aZero::Editor
{
	class Editor
	{
	public:
		Editor()
		{
			aZero::Window::Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);

			SDL_Vulkan_LoadLibrary(NULL);

			uint32_t sdlExtensionCount = 0;
			char const* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
			std::vector<const char*> extensions;
			extensions.reserve(sdlExtensionCount + 1);
			for (uint32_t i = 0; i < sdlExtensionCount; i++)
			{
				extensions.push_back(sdlExtensions[i]);
			}

			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

			Engine::Desc engineDesc{
				.FrameBufferCount = 3,
				.ProjectRootDirectory = PROJECT_DIRECTORY,
				.Vk_Extensions = extensions,
				.EnableValidationLayer = true,
				.EnableValidationLayerWarnings = false
			};
			m_Engine = std::make_unique<Engine>(engineDesc);

			m_MainWindow = std::make_unique<RenderWindow>(Window::WindowDesc("aZero Engine", { 0,0,1920,1080 }, { 1,1,0,1 }, SDL_WINDOW_RESIZABLE, {}), m_Engine->GetCore());

			RenderAPI::Image::Desc ctDesc{
				.Format = VkFormat::VK_FORMAT_R8G8B8A8_SRGB,
				.Type = VkImageType::VK_IMAGE_TYPE_2D,
				.Usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
				.Width = (uint32_t)m_MainWindow->GetClientDimensions()[0],
				.Height = (uint32_t)m_MainWindow->GetClientDimensions()[1],
				.Depth = 1,
				.MipCount = 1,
				.ArrayLayers = 1
			};
			m_ColorTarget.Init(m_Engine->GetCore(), ctDesc);

			RenderAPI::DepthStencilImage::Desc dsvDesc{
				.Width = (uint32_t)m_MainWindow->GetClientDimensions()[0],
				.Height = (uint32_t)m_MainWindow->GetClientDimensions()[1]
			};
			m_DepthStencilTarget.Init(m_Engine->GetCore(), dsvDesc);

			aZero::RenderAPI::ImageView::Desc rtvView{
				.Image = &m_ColorTarget,
				.Type = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D,
				.ImageAspect = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
			};
			m_ColorView = aZero::RenderAPI::ImageView(m_Engine->GetCore(), rtvView);
		}

		~Editor()
		{
			aZero::Window::Shutdown();
		}

		void Run()
		{
			while (m_MainWindow->IsOpen())
			{
				this->Update();
			}
		}

		void Update()
		{
			m_MainWindow->HandleEvents();

			while (!m_Engine->GetRenderer().NextFrame(UINT64_MAX))
			{
				// do work while waiting
			}
			m_MainWindow->m_SwapChain.NextFrame(m_Engine->GetRenderer().GetFrameIndex());

			Renderer::Renderer::RenderDesc renderDesc{
				.ColorTarget = &m_ColorTarget,
				.DepthStencil = &m_DepthStencilTarget,
				.ColorView = &m_ColorView
			};
			m_Engine->GetRenderer().Render(renderDesc);

			m_Engine->GetRenderer().PresentSwapchain(m_ColorTarget, m_MainWindow->m_SwapChain);
		}

	private:

		RenderAPI::Image m_ColorTarget;
		aZero::RenderAPI::ImageView m_ColorView;
		RenderAPI::DepthStencilImage m_DepthStencilTarget;
		std::unique_ptr<Engine> m_Engine;
		std::unique_ptr<RenderWindow> m_MainWindow;
		uint64_t m_FrameIndex = 0;
		Input::KeyboardListener m_KeyboardListener;
	};
}