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

			static constexpr auto supportedExtensions =
				fastgltf::Extensions::KHR_mesh_quantization |
				fastgltf::Extensions::KHR_texture_transform |
				fastgltf::Extensions::KHR_materials_variants |
				fastgltf::Extensions::KHR_lights_punctual |
				fastgltf::Extensions::KHR_texture_basisu
				;

			fastgltf::Parser parser(supportedExtensions);

			constexpr auto gltfOptions =
				fastgltf::Options::DontRequireValidAssetMember |
				fastgltf::Options::AllowDouble |
				fastgltf::Options::LoadExternalBuffers |
				//fastgltf::Options::LoadExternalImages |
				fastgltf::Options::GenerateMeshIndices |
				fastgltf::Options::DecomposeNodeMatrices
				;

			const std::filesystem::path path(std::string(PROJECT_DIRECTORY) + "KtxCube.gltf");
			auto gltfFile = fastgltf::MappedGltfFile::FromPath(path);
			if (!bool(gltfFile)) {
				std::cerr << "Failed to open glTF file: " << fastgltf::getErrorMessage(gltfFile.error()) << '\n';
				throw;
			}

			auto loadedAsset = parser.loadGltf(gltfFile.get(), path.parent_path(), gltfOptions);
			fastgltf::Asset* asset = loadedAsset.get_if();
			if (!asset) {
				std::cerr << "Failed to load glTF file: " << fastgltf::getErrorMessage(loadedAsset.error()) << '\n';
				throw;
			}

			auto& meshSystem = m_Engine->GetMeshSystem();
			auto& textureSystem = m_Engine->GetTextureSystem();
			auto& materialSystem = m_Engine->GetMaterialSystem();

			Asset::ImageData imageData = aZero::File::ExtractGltfImage(asset->images[0], asset, path);
			auto texture = textureSystem.CreateTexture("MyTexture", imageData);
			if (texture)
			{
				Asset::MaterialDefinition def{
				.Parameters = {
					{"float1", Asset::ParameterType::float1},
					{"int3", Asset::ParameterType::int3},
					{"texture", Asset::ParameterType::tex2D}
				},
				.Properties = 0,
				.MaterialShaderPath = std::string(ENGINE_SHADER_DIRECTORY) + "/testMaterial.slang",
				.MaterialShaderName = "TestMaterial2"
				};

				auto material = materialSystem.CreateMaterial("MyMaterialType", def);
				if (material)
				{
					auto materialInstance = materialSystem.CreateMaterialInstance("MyMaterialInstance", material->get());
					if (materialInstance)
					{
						materialInstance->get().SetParameter("float1", 6.7f);
						materialInstance->get().SetParameter("int3", glm::ivec3(1, 2, 3));
						materialInstance->get().SetParameter("texture", texture->get().GetDescriptorIndex());

						m_Engine->GetRenderer().testMaterialPtr = materialInstance->get().GetBDA();

						Asset::MeshletMeshData meshData = aZero::File::ExtractGltfMeshletMesh(asset->meshes[0], asset);
						auto mesh = meshSystem.CreateMesh("MyMesh", meshData);
						if (mesh)
						{
							// todo Create test entity that will get rendered
							int x = 2;
						}
					}
				}
				texture->get().SetData(imageData);
			}

			m_Engine->GetRenderer().FinalizeFrame();
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