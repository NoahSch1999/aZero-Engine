#pragma once
#include "src/RenderWindow.hpp"
#include <SDL3/SDL_vulkan.h>
#include "asset/loading_saving/Gltf.hpp"

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
					.Falloff = 0.f,
					.Intensity = 1.f
				};
				ent.set(pLight);
				ent.set(p);
				ent.set(r);
				ent.set(s);
				Component::MeshComponent m(m_Engine->GetMeshSystem().GetMesh("MyMesh").value().get(), m_Engine->GetMaterialSystem().GetMaterialInstance("MyMaterialInstance").value().get());
				ent.set(m);
			}

			{
				auto cam = m_Scene->GetWorld().entity("camera2");
				Component::CameraComponent camera{
						.m_Type = Component::ECameraType::PERSPECTIVE,
						.m_PriorityIndex = 0,
						.m_Fov = M_PI - 1,
						.m_Top = 0.5f,
						.m_Left = 0.5f,
						.m_Bottom = 1.f,
						.m_Right = 1.f,
						.m_Near = 0.001f,
						.m_Far = 1000.f,
						.m_OptColorTarget = m_ColorTarget,
						.m_OptDepthStencilTarget = m_DepthStencilTarget,
						.m_ClearColorTargetOnRender = false,
						.m_ClearDepthStencilTargetOnRender = false
				};
				p.Position.z = 5;
				cam.set(p);
				cam.set(r);
				cam.set(camera);
				cam.add<Component::StaticComponent>();
			}

			m_Scene = std::make_unique<Scene::Scene>();
			m_Scene->LoadGltf(std::string(PROJECT_DIRECTORY) + "scenes/sponza/sponza.gltf", m_Engine->GetMeshSystem(), m_Engine->GetMaterialSystem(), m_Engine->GetTextureSystem());


			{
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


			{
				auto ent = m_Scene->GetWorld().entity("mesh2");
				p.Position = { 0,0.3,0 };
				r.Rotation = { 0,45,45 };
				s.Scale = { 1,1,1 };
				ent.set(p);
				ent.set(r);
				ent.set(s);
				Component::MeshComponent m(m_Engine->GetMeshSystem().GetMesh("MyMesh").value().get(), m_Engine->GetMaterialSystem().GetMaterialInstance("MyMaterialInstance").value().get());
				ent.set(m);
				ent.add<Component::StaticComponent>();
			}
			m_Engine->GetRenderer().FinalizeFrame();
		}

		~Editor()
		{
			m_Scene.reset();
			m_Engine->GetRenderer().FlushRenderingCommands();
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
			while (!m_Engine->GetRenderer().NextFrame(UINT64_MAX))
			{
				// do work while waiting
			}

			m_MainWindow->HandleEvents();

			m_MainWindow->m_SwapChain.NextFrame(m_Engine->GetRenderer().GetFrameIndex());

			// do frame stuff

			/*auto ent = m_Scene->GetWorld().entity("mesh3");
			ent.get_mut<Component::RotationComponent>().Rotation.y += 0.1f;
			ent.get_mut<Component::PositionComponent>().Position.z = sin(idk) * 4;*/
			auto cam = m_Scene->GetWorld().entity("camera");
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
			m_Scene->MarkStaticMeshesDirty();

			m_Engine->GetRenderer().FinalizeFrame();

			m_Engine->GetRenderer().RenderScene(*m_Scene.get());

			m_Engine->GetRenderer().CopyToSwapchain(m_ColorTarget, m_MainWindow->m_SwapChain);

			m_MainWindow->m_SwapChain.Present();
		}

	private:
		flecs::entity cameraEnt;
		flecs::entity meshEnt;
		std::unique_ptr<aZero::Scene::Scene> m_Scene;
		std::unique_ptr<Engine> m_Engine;
		Renderer::RenderTarget m_ColorTarget;
		Renderer::RenderTarget m_DepthStencilTarget;
		std::unique_ptr<RenderWindow> m_MainWindow;
		uint64_t m_FrameIndex = 0;
		Input::KeyboardListener m_KeyboardListener;
	};
}