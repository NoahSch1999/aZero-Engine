#include "MaterialInstance.hpp"
#include "renderer/system/MaterialSystem.hpp"

aZero::Asset::MaterialInstance::MaterialInstance(MaterialType& diMaterial, const std::string& name, MaterialInstanceBDA instanceBDA, System::MaterialSystem& diMaterialSystem)
	:m_Name(name), m_diMaterial(&diMaterial), m_InstanceBDA(instanceBDA), m_diMaterialSystem(&diMaterialSystem)
{
	m_Data.resize(diMaterial.GetParameterSizeInfo().TotalSize);
}

void aZero::Asset::MaterialInstance::SetDirty()
{
	if (m_diMaterialSystem)
	{
		m_diMaterialSystem->SetMaterialInstanceDirty(*this);
	}
}