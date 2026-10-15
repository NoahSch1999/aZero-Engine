#include "Engine.hpp"

namespace aZero
{
	Engine::Engine(const Engine::Desc& desc)
	{
		RenderAPI::Core::Desc coreDesc{
			.ApplicationName = "aZeroEngine_APP",
			.Extensions = desc.Vk_Extensions,
			.EnableValidationLayer = desc.EnableValidationLayer,
			.EnableValidationLayerWarnings = desc.EnableValidationLayerWarnings
		};

		m_Core = std::make_unique<RenderAPI::Core>();
		if (!m_Core->Init(coreDesc))
		{
			return;
		}

		m_Compiler = std::make_unique<Compiler::Compiler>(*m_Core.get());

		m_ProjectRootDirectory = desc.ProjectRootDirectory;
		m_AudioEngine = std::make_unique<Audio::AudioEngine>();
		m_Renderer = std::make_unique<Renderer::Renderer>(*m_Core.get(), *m_Compiler.get(), desc.FrameBufferCount);

		//m_PhysicsEngine = std::make_unique<Physics::PhysicsEngine>();
		//m_AssetManager = std::make_unique<Asset::AssetManager<std::string>>(*m_Renderer.get(), m_ProjectRootDirectory + "assets/");
	}

	Engine::~Engine()
	{
		m_Core->FlushGPU();
	}
}