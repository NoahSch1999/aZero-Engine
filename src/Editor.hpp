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

			m_MainWindow = std::make_unique<RenderWindow>(Window::WindowDesc("aZero Engine", { 0,0,1920,1080 }, { 1,1,0,1 }, SDL_WINDOW_RESIZABLE, {}), m_Engine->GetCore(), m_Engine->GetRenderer());

			auto cTargetRes = m_Engine->GetRenderer().CreateRenderTarget({
				(uint32_t)m_MainWindow->GetClientDimensions()[0],
				(uint32_t)m_MainWindow->GetClientDimensions()[1]
				},
				aZero::Renderer::ERenderTargetType::COLOR
				);
			if (!cTargetRes.has_value())
			{
				throw;
			}

			m_ColorTarget = std::move(cTargetRes.value());

			auto dsvTargetRes = m_Engine->GetRenderer().CreateRenderTarget({
				(uint32_t)m_MainWindow->GetClientDimensions()[0],
				(uint32_t)m_MainWindow->GetClientDimensions()[1]
				},
				aZero::Renderer::ERenderTargetType::DEPTH_STENCIL
			);
			if (!dsvTargetRes.has_value())
			{
				throw;
			}
			m_DepthStencilTarget = std::move(dsvTargetRes.value());
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
			while (!m_Engine->GetRenderer().NextFrame(UINT64_MAX))
			{
				// do work while waiting
			}

			m_MainWindow->HandleEvents();

			m_MainWindow->m_SwapChain.NextFrame(m_Engine->GetRenderer().GetFrameIndex());

			// do frame stuff

			m_Engine->GetRenderer().FinalizeFrame();

			Renderer::Renderer::RenderDesc renderDesc{
				.ColorTarget = &m_ColorTarget,
				.DepthStencil = &m_DepthStencilTarget
			};
			m_Engine->GetRenderer().Render(renderDesc);

			m_Engine->GetRenderer().CopyToSwapchain(m_ColorTarget, m_MainWindow->m_SwapChain);

			m_MainWindow->m_SwapChain.Present();
		}

	private:

		Renderer::RenderTarget m_ColorTarget;
		Renderer::RenderTarget m_DepthStencilTarget;
		std::unique_ptr<Engine> m_Engine;
		std::unique_ptr<RenderWindow> m_MainWindow;
		uint64_t m_FrameIndex = 0;
		Input::KeyboardListener m_KeyboardListener;
	};
}