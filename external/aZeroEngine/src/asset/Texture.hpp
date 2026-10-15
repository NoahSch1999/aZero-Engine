#pragma once
#include "RenderAssetBase.hpp"
#include "AssetData.hpp"

namespace aZero::Asset
{
	struct TextureRenderRef
	{
		uint32_t DescriptorIndex = std::numeric_limits<uint32_t>::max();
		bool IsValid() const {
			return DescriptorIndex != std::numeric_limits<uint32_t>::max();
		}
	};

	class Texture : public RenderAssetBase<TextureRenderRef, aZero::Asset::ImageData>
	{
	public:
		Texture() = default;
		Texture(aZero::Asset::ImageData&& data)
			:RenderAssetBase(std::move(data)) {
		}
		Texture(const aZero::Asset::ImageData& data)
			:RenderAssetBase(data) {
		}

		void ClearCachedData() { m_CachedData.Data.clear(); }

	private:

	};
}