#pragma once

namespace aZero::Renderer { class Renderer; };

namespace aZero::Asset
{
	template<typename RenderRef, typename Data>
	class RenderAssetBase
	{
		friend class aZero::Renderer::Renderer;
	public:
		RenderAssetBase() = default;
		RenderAssetBase(Data&& data)
			:m_CachedData(std::move(data)) {
		}
		RenderAssetBase(const Data& data)
			:m_CachedData(data) {}

		const RenderRef& GetRenderRef() const { return m_RenderRef; }
		const Data& GetCachedData() const { return m_CachedData; }
		RenderRef m_RenderRef;
	private:
	protected:
		Data m_CachedData;
	};
}