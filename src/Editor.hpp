#pragma once
#include "src/RenderWindow.hpp"
#include <SDL3/SDL_vulkan.h>
#include "asset/loading_saving/Gltf.hpp"
#include "ImguiInclude.hpp"

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

			m_Engine->GetRenderer().NextFrame();

			m_MainWindow = std::make_unique<RenderWindow>(Window::WindowDesc("aZero Engine", { 0,0,1920,1080 }, { 1,1,0,1 }, SDL_WINDOW_RESIZABLE, {}), m_Engine->GetCore(), m_Engine->GetRenderer());

			m_Imgui.Init(m_Engine->GetCore(), m_Engine->GetRenderer(), 
				m_MainWindow->GetSDLWindow(), m_MainWindow->GetVkSurfaceKHR(), 
				m_MainWindow->GetClientDimensions()[0], m_MainWindow->GetClientDimensions()[1],
				m_MainWindow->m_SwapChain.GetBackbufferCount(),
				VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT
			);

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


			// ---------------------------------------
			m_Scene = std::make_unique<Scene::Scene>();
			Component::PositionComponent p;
			Component::RotationComponent r;
			Component::ScaleComponent s;

			{
				auto ent = m_Scene->GetWorld().entity("mesh");
				p.Position = { 0,0,0 };
				r.Rotation = { 0,0,0 };
				s.Scale = { 1,1,1 };
				
				ent.set(p);
				ent.set(r);
				ent.set(s);
				Component::MeshComponent m(m_Engine->GetMeshSystem().GetMesh("MyMesh").value().get(), m_Engine->GetMaterialSystem().GetMaterialInstance("MyMaterialInstance").value().get());
				ent.set(m);
				ent.add<Component::StaticComponent>();
			}

			{
				auto ent = m_Scene->GetWorld().entity("mesh3");
				p.Position = { 0,6,1 };
				r.Rotation = { 0,0,0 };
				s.Scale = { 0.5,0.5,0.5 };

				Component::PointLightComponent pLight{
					.Color = glm::vec3(1, 1, 1),
					.Range = 2.f,
					.Intensity = 1.f
				};
				ent.set(pLight);
				ent.set(p);
				ent.set(r);
				ent.set(s);
				Component::MeshComponent m(m_Engine->GetMeshSystem().GetMesh("MyMesh").value().get(), m_Engine->GetMaterialSystem().GetMaterialInstance("MyMaterialInstance").value().get());
				ent.set(m);
			}

			CurrentLoadingHandle = std::async(std::launch::async, [&] {
				CurrentGltf.Load(std::string(PROJECT_DIRECTORY) + "scenes/sponza/sponza2.gltf");
				//CurrentGltf.Load(std::string(PROJECT_DIRECTORY) + "scenes/test.gltf");
			});

			{
				p.Position = { 0,0,5 };
				r.Rotation = { 0,0,0 };
				auto cam = m_Scene->GetWorld().entity("camera");
				Component::CameraComponent camera{
						.m_Type = Component::ECameraType::PERSPECTIVE,
						.m_PriorityIndex = 1,
						.m_Fov = M_PI / 3.f,
						.m_Top = 0.0f,
						.m_Left = 0.0f,
						.m_Bottom = 1.f,
						.m_Right = 1.f,
						.m_Near = 0.001f,
						.m_Far = 1000.f,
						.m_OptColorTarget = m_ColorTarget,
						.m_OptDepthStencilTarget = m_DepthStencilTarget,
						.m_ClearColorTargetOnRender = true,
						.m_ClearDepthStencilTargetOnRender = true
				};
				cam.set(p);
				cam.set(r);
				cam.set(camera);
				cam.add<Component::StaticComponent>();
			}

			{
				auto ent = m_Scene->GetWorld().entity("mesh2");
				p.Position = { 0,0.3,0 };
				r.Rotation = { 0,45,45 };
				s.Scale = { 1,1,1 };
				ent.set(p);
				ent.set(r);
				ent.set(s);
				Component::MeshComponent m(m_Engine->GetMeshSystem().GetMesh("MyMesh").value().get(), m_Engine->GetMaterialSystem().GetMaterialInstance("MyMaterialInstance").value().get());
				//Component::MeshComponent m(m_Engine->GetMeshSystem().GetMesh("MyMesh").value().get(), m_Engine->GetMaterialSystem().GetMaterialInstance(System::MaterialSystem::s_DefaultPBRInstanceStr).value().get());
				ent.set(m);
				ent.add<Component::StaticComponent>();
			}

			m_Engine->GetRenderer().FinalizeFrame();
		}

		~Editor()
		{
			m_Scene.reset();
			m_Engine->GetRenderer().FlushRenderingCommands();
			m_Imgui.Shutdown(m_Engine->GetCore());
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
			static float idk = 0.01f;
			static float exposure = 1.f;
			static aZero::Renderer::ETonemapType tonemap = aZero::Renderer::ETonemapType::REINHARD;
			while (!m_Engine->GetRenderer().NextFrame(UINT64_MAX))
			{
				// do work while waiting
			}

			m_MainWindow->HandleEvents();

			m_Imgui.BeginFrame();

			//m_MainWindow->m_SwapChain.NextFrame(m_Engine->GetRenderer().GetFrameIndex());

			// do frame stuff
			if (CurrentLoadingHandle.valid() && CurrentLoadingHandle.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
			{

				std::cout << "\n------------------------------------\nInitializing scene\n------------------------------------\n";
				std::chrono::high_resolution_clock clock;
				auto start = clock.now();

				m_Scene = std::make_unique<Scene::Scene>();
				m_Scene->FromGltf(CurrentGltf, m_Engine->GetMeshSystem(), m_Engine->GetMaterialSystem(), m_Engine->GetTextureSystem());

				{
					Component::PositionComponent p;
					Component::RotationComponent r;
					p.Position = { 0,0,1 };
					r.Rotation = { 0,0,0 };
					auto cam = m_Scene->GetWorld().entity("camera");
					Component::CameraComponent camera{
							.m_Type = Component::ECameraType::PERSPECTIVE,
							.m_PriorityIndex = 1,
							.m_Fov = M_PI / 3.f,
							.m_Top = 0.0f,
							.m_Left = 0.0f,
							.m_Bottom = 1.f,
							.m_Right = 1.f,
							.m_Near = 0.001f,
							.m_Far = 1000.f,
							.m_OptColorTarget = m_ColorTarget,
							.m_OptDepthStencilTarget = m_DepthStencilTarget,
							.m_ClearColorTargetOnRender = true,
							.m_ClearDepthStencilTargetOnRender = true
					};
					cam.set(p);
					cam.set(r);
					cam.set(camera);
					cam.add<Component::StaticComponent>();
				}

				auto end = clock.now();
				std::chrono::duration<double> diff = end - start;
				std::cout << "\n------------------------------------\nCompleted after: " << diff.count() << " seconds\n------------------------------------\n";
				CurrentLoadingHandle = {};
			}
			static bool ssss = false;
			auto cam = m_Scene->GetWorld().entity("camera");
			if (cam.is_alive())
			{
				if (GetAsyncKeyState('W'))
				{
					cam.get_mut<Component::PositionComponent>().Position.z -= idk;
				}
				if (GetAsyncKeyState('S'))
				{
					cam.get_mut<Component::PositionComponent>().Position.z += idk;
				}
				if (GetAsyncKeyState('A'))
				{
					cam.get_mut<Component::PositionComponent>().Position.x -= idk;
				}
				if (GetAsyncKeyState('D'))
				{
					cam.get_mut<Component::PositionComponent>().Position.x += idk;
				}
				if (GetAsyncKeyState(VK_SPACE))
				{
					cam.get_mut<Component::PositionComponent>().Position.y += idk;
				}
				if (GetAsyncKeyState(VK_LSHIFT))
				{
					cam.get_mut<Component::PositionComponent>().Position.y -= idk;
				}
				if (GetAsyncKeyState('M'))
				{
					m_Engine->GetMaterialSystem().GetMaterialInstance("MyMaterialInstance")->get().SetParameter("float1", 0.f);
					ssss = true;
				}
				if (GetAsyncKeyState('N'))
				{
					m_Engine->GetMaterialSystem().GetMaterialInstance("MyMaterialInstance")->get().SetParameter("float1", 1.f);
					ssss = false;
				}
				if (GetAsyncKeyState('9'))
				{
					exposure -= 0.001f;
					exposure = std::max(exposure, 0.f);
				}
				if (GetAsyncKeyState('0'))
				{
					exposure += 0.001f;
				}

				if (GetAsyncKeyState('1'))
				{
					tonemap = aZero::Renderer::ETonemapType::NONE;
				}
				if (GetAsyncKeyState('2'))
				{
					tonemap = aZero::Renderer::ETonemapType::REINHARD;
				}
				if (GetAsyncKeyState('3'))
				{
					tonemap = aZero::Renderer::ETonemapType::ACES;
				}
			}
			
			m_Scene->MarkStaticMeshesDirty();

			m_Engine->GetRenderer().FinalizeFrame();

			m_Engine->GetRenderer().RenderScene(*m_Scene.get());

			m_Imgui.Render(m_Engine->GetRenderer(), m_ColorTarget.GetImage(), m_ColorTarget.GetImageView());

			if(!ssss)
				m_Engine->GetRenderer().PresentSwapchain(m_ColorTarget, m_MainWindow->m_SwapChain, exposure, tonemap);
		}

	private:
		aZero::Asset::LoadedGltf CurrentGltf;
		std::future<void> CurrentLoadingHandle;
		flecs::entity cameraEnt;
		flecs::entity meshEnt;
		std::unique_ptr<aZero::Scene::Scene> m_Scene;
		std::unique_ptr<Engine> m_Engine;
		Renderer::RenderTarget m_ColorTarget;
		Renderer::RenderTarget m_DepthStencilTarget;
		std::unique_ptr<RenderWindow> m_MainWindow;
		uint64_t m_FrameIndex = 0;
		Input::KeyboardListener m_KeyboardListener;
		aZero::ImGui_Wrapper m_Imgui;
	};
}