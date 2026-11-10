#pragma once
#include <memory>
#include <aZeroAudio.hpp>
#include "physics/PhysicsEngine.hpp"
#include "render_api/Core.hpp"
#include "renderer/Renderer.hpp"
#include "misc/NonMoveCopy.hpp"

#include "asset/loading_saving/gltf.hpp"

namespace aZero
{
	class Engine : public NonCopyable
	{
	public:
		struct Desc
		{
			uint32_t FrameBufferCount;
			std::string ProjectRootDirectory;
			std::vector<const char*> Vk_Extensions;
			bool EnableValidationLayer = false;
			bool EnableValidationLayerWarnings = false;
		};
		Engine(const Desc& desc);
		Engine(Engine&&) noexcept = default;
		Engine& operator=(Engine&&) noexcept = default;
		~Engine();

		RenderAPI::Core& GetCore() { return *m_Core.get(); }

		Compiler::Compiler& GetCompiler() const { return *m_Compiler.get(); }
		std::string GetProjectRootDirectory() { return m_ProjectRootDirectory; }
		Audio::AudioEngine& GetAudioEngine() const { return *m_AudioEngine.get(); }
		Physics::PhysicsEngine& GetPhysicsEngine() const { return *m_PhysicsEngine.get(); }

		Renderer::Renderer& GetRenderer() const { return *m_Renderer.get(); }
		System::MeshSystem& GetMeshSystem() const { return *m_Renderer->m_MeshSystem.get(); }
		System::TextureSystem&	GetTextureSystem() const { return *m_Renderer->m_TextureSystem.get(); }
		System::MaterialSystem& GetMaterialSystem() const { return *m_Renderer->m_MaterialSystem.get(); }

	private:
		std::unique_ptr<RenderAPI::Core> m_Core;
		std::unique_ptr<Compiler::Compiler> m_Compiler;
		std::string m_ProjectRootDirectory;
		std::unique_ptr<Audio::AudioEngine> m_AudioEngine;
		std::unique_ptr<Physics::PhysicsEngine> m_PhysicsEngine;
		std::unique_ptr<Renderer::Renderer> m_Renderer;
		System::MeshSystem m_MeshSystem;
		System::TextureSystem m_TextureSystem;
		System::MaterialSystem m_MaterialSystem;
	};
}