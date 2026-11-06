#pragma once

namespace aZero::Renderer { class Renderer; class GpuMeshSystem; class GpuTextureSystem; class GpuMaterialSystem; };

namespace aZero::Asset
{
	template<typename RenderRef, typename Data>
	class RenderAssetBase
	{
		friend class aZero::Renderer::Renderer;
		friend class aZero::Renderer::GpuMeshSystem;
		friend class aZero::Renderer::GpuTextureSystem;
		friend class aZero::Renderer::GpuMaterialSystem;
	public:
		RenderAssetBase() = default;
		RenderAssetBase(Data&& data)
			:m_CachedData(std::move(data)) {
		}
		RenderAssetBase(const Data& data)
			:m_CachedData(data) {}

		const RenderRef& GetRenderRef() const { return m_RenderRef; }
		const Data& GetCachedData() const { return m_CachedData; }
		RenderRef m_RenderRef; // todo Make private
	private:
	protected:
		Data m_CachedData;
	};
}