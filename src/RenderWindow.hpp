#pragma once
#include <aZeroWindow.hpp>
#include <aZeroInput.hpp>
#include "ImguiInclude.hpp"

#include "render_api/Core.hpp"
#include "renderer/Renderer.hpp"
#include "SDL3/SDL_Vulkan.h"

class RenderWindow : public aZero::Window::Window_Win32 {
public:
	RenderWindow() = default;
	explicit RenderWindow(const aZero::Window::WindowDesc& desc, aZero::RenderAPI::Core& core, aZero::Renderer::Renderer& diRenderer)
		:Window_Win32(desc)
	{
		SDL_Vulkan_CreateSurface(this->GetSDLWindow(), core.GetInstance(), nullptr, &m_Surface);

		aZero::RenderAPI::Swapchain::Desc swapChainDesc{
			.Surface = m_Surface,
			.Format = VkFormat::VK_FORMAT_R8G8B8A8_SRGB,
			.FramesInFlight = 3,
			.Width = (uint32_t)this->GetClientDimensions()[0],
			.Height = (uint32_t)this->GetClientDimensions()[1]
		};
		m_SwapChain.Init(core, swapChainDesc);
		m_DeviceManager.ReloadDevices();
		m_diRenderer = &diRenderer;
	}

	//bool WaitOnSwapchain() { return WaitForSingleObject(m_WaitableHandle, 0) == WAIT_OBJECT_0; }
	//void Present() { m_SwapChain.Present(); }
	void HandleEvents() { 
		m_DeviceManager.UpdateDeviceStates();
		this->PollEvents();
	}

	aZero::Input::DeviceManager& GetDeviceManager() { return m_DeviceManager; }

	VkImage GetCurrentBackbuffer() { return m_SwapChain.GetCurrentBackBuffer(); }
	//aZero::RenderAPI::SwapChain& GetSwapChain() { return m_SwapChain; }

	aZero::RenderAPI::Swapchain m_SwapChain;
private:
	void PollEventImpl(const SDL_Event& event) final {
		if (event.type == SDL_EVENT_QUIT) {
			this->Close();
		}
		else if (event.type == SDL_EVENT_KEY_UP)
		{
			if (event.key.key == SDLK_1)
			{
				m_diRenderer->Test(0, 0, 1.f);
			}
			else if (event.key.key == SDLK_2)
			{
				m_diRenderer->Test(0.f, 1.f, 0.f);
			}
			
		}
		//ImGui_ImplSDL3_ProcessEvent(&event);
		m_DeviceManager.ProcessEvent(event);
	}

	aZero::Renderer::Renderer* m_diRenderer = nullptr;
	VkSurfaceKHR m_Surface;
	aZero::Input::DeviceManager m_DeviceManager;
};