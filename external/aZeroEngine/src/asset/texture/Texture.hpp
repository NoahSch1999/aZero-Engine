#include "asset/AssetData.hpp"
#include "render_api/Descriptor.hpp"

namespace aZero::System { class TextureSystem; }

namespace aZero::Asset
{
	struct ImageData : public AssetBase
	{
		struct Mip
		{
			std::uint32_t Offset;
			std::uint32_t RowPitch;
			std::uint32_t SlicePitch;
		};

		uint32_t BaseWidth;
		uint32_t BaseHeight;
		uint32_t FaceCount;
		uint32_t DepthOrArrayCount;
		bool IsArray = false;
		std::vector<std::byte> Data;
		std::vector<Mip> MipLevels;
		VkFormat Format = VkFormat::VK_FORMAT_UNDEFINED;
		VkImageType Type = VkImageType::VK_IMAGE_TYPE_MAX_ENUM;

		bool HasValidData() const
		{
			return BaseWidth > 0 && BaseHeight > 0 && DepthOrArrayCount > 0 && FaceCount > 0 && !Data.empty() && !MipLevels.empty() && Format != VkFormat::VK_FORMAT_UNDEFINED && Type != VkImageType::VK_IMAGE_TYPE_MAX_ENUM;
		}
	};

	class Texture : public Asset::AssetBase
	{
		friend class System::TextureSystem;
	public:
		Texture() = default;
		Texture(const std::string& name)
		{
			Name = name;
		}
		void SetData(const ImageData& data) { m_Data = data; m_IsDirty = true; }
		void SetNotDirty() { m_IsDirty = false; }
		bool IsDirty() const { return m_IsDirty; }
		RenderAPI::DescriptorIndex GetDescriptorIndex() const { return m_DescriptorIndex; }
	private:
		ImageData m_Data;
		RenderAPI::DescriptorIndex m_DescriptorIndex = RenderAPI::g_InvalidDescriptorIndex;
		bool m_IsDirty = true;
	};
}