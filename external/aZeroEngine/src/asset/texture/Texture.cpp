#include "Texture.hpp"
#include "renderer/system/TextureSystem.hpp"

aZero::Asset::Texture::Texture(const std::string& name, System::TextureSystem& diTextureSystem)
	:m_diTextureSystem(&diTextureSystem)
{
	Name = name;
}

void aZero::Asset::Texture::SetDirty()
{
	if (m_diTextureSystem)
	{
		m_diTextureSystem->SetTextureDirty(*this);
	}
}