#include "Mesh.hpp"
#include "renderer/system/MeshSystem.hpp"

aZero::Asset::Mesh::Mesh(const std::variant<Asset::LegacyMeshData, Asset::MeshletMeshData>& data, System::MeshSystem& diMeshSystem)
	:m_Data(data), m_diMeshSystem(&diMeshSystem)
{

}

void aZero::Asset::Mesh::SetDirty()
{
	if (m_diMeshSystem)
	{
		m_diMeshSystem->SetMeshDirty(*this);
	}
}