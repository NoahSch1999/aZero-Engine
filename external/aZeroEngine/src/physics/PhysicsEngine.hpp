#pragma once
#include "PhysicsWorld.hpp"
#include <utility>
#include "misc/NonMoveCopy.hpp"

namespace aZero
{
	namespace Rendering { class WireframeRenderer; }
	namespace Physics
	{
		class PhysicsWorld;

		namespace Layers
		{
			static constexpr JPH::ObjectLayer STATIC = 0;
			static constexpr JPH::ObjectLayer DYNAMIC = 1;
			static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
		};

		namespace BroadPhaseLayers
		{
			static constexpr JPH::BroadPhaseLayer STATIC(0);
			static constexpr JPH::BroadPhaseLayer DYNAMIC(1);
			static constexpr uint32_t NUM_LAYERS(2);
		};

		class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
		{
		public:
			BPLayerInterfaceImpl();
			virtual uint32_t GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
			virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override { return m_ObjectToBroadPhase[inLayer]; }
		
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
			virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override;
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

		private:
			JPH::BroadPhaseLayer m_ObjectToBroadPhase[Layers::NUM_LAYERS];
		};

		class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
		{
		public:
			virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override;
		};

		class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
		{
		public:
			virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override;
		};

		class PhysicsEngine
		{
		public:
			PhysicsEngine();
			~PhysicsEngine();
			PhysicsEngine(PhysicsEngine&& other) noexcept
			{
				*this = std::move(other);
			}

			PhysicsEngine& operator=(PhysicsEngine&& other) noexcept
			{
				std::swap(m_JobSystem, other.m_JobSystem);
				std::swap(m_BroadPhaseLayerInterface, other.m_BroadPhaseLayerInterface);
				std::swap(m_ObjectVsBroadPhaseLayerInterface, other.m_ObjectVsBroadPhaseLayerInterface);
				std::swap(m_ObjectLayerPairInterface, other.m_ObjectLayerPairInterface);
				return *this;
			}
			void CreateWorld(PhysicsWorld& world, float updateFrequency = 1.0f / 60.f, uint32_t maxBodies = 1024, uint32_t maxBodyPairs = 1024, uint32_t maxContactConstraints = 1024, uint32_t maxBodyMutexes = 0);

		private:
			std::unique_ptr<JPH::JobSystemThreadPool> m_JobSystem;
			std::unique_ptr<BPLayerInterfaceImpl> m_BroadPhaseLayerInterface;
			std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> m_ObjectVsBroadPhaseLayerInterface;
			std::unique_ptr<ObjectLayerPairFilterImpl> m_ObjectLayerPairInterface;
		};
	}
}