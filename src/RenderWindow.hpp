#pragma once
#include <aZeroWindow.hpp>
#include <aZeroInput.hpp>
#include "ImguiInclude.hpp"

#include "render_api/Core.hpp"
#include "render_api/SwapChain.hpp"
#include "SDL3/SDL_Vulkan.h"

class RenderWindow : public aZero::Window::Window_Win32 {
public:
	RenderWindow() = default;
	explicit RenderWindow(const aZero::Window::WindowDesc& desc, aZero::RenderAPI::Core& core)
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
		//ImGui_ImplSDL3_ProcessEvent(&event);
		m_DeviceManager.ProcessEvent(event);
	}

	VkSurfaceKHR m_Surface;
	aZero::Input::DeviceManager m_DeviceManager;
};