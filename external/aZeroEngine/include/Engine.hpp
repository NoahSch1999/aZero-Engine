#pragma once
#include <memory>
#include <aZeroAudio.hpp>
#include "physics/PhysicsEngine.hpp"
#include "render_api/Core.hpp"
#include "renderer/Renderer.hpp"
#include "misc/NonMoveCopy.hpp"

#include "asset/gltf.hpp"
#include "asset/AssetManager.hpp"

namespace aZero
{
	namespace Asset
	{
		template<typename T>
		using AssetManager = aZero::Asset::AssetManagerT<T, aZero::Asset::Mesh, aZero::Asset::Material, aZero::Asset::Texture>;
	}

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
		Asset::AssetManager<std::string>& GetAssetManager() const { return *m_AssetManager.get(); }

	private:
		std::unique_ptr<RenderAPI::Core> m_Core;
		std::unique_ptr<Compiler::Compiler> m_Compiler;
		std::string m_ProjectRootDirectory;
		std::unique_ptr<Audio::AudioEngine> m_AudioEngine;
		std::unique_ptr<Physics::PhysicsEngine> m_PhysicsEngine;
		std::unique_ptr<Renderer::Renderer> m_Renderer;
		std::unique_ptr<Asset::AssetManager<std::string>> m_AssetManager;
	};
}