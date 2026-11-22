#pragma once
#include "misc/Helpers.hpp"
#include "misc/BoundingVolume.hpp"
#include "asset/Mesh/Mesh.hpp"
#include "asset/Material/MaterialInstance.hpp"

namespace aZero::Component
{
    class MeshComponent {
    public:
        static constexpr uint32_t s_MaxNumberOfSubmeshes = 10;

        struct Submesh
        {
            VkDeviceAddress MaterialPtr;
            uint32_t MetadataIndex;
            uint32_t MaterialTypeIndex;
            const Asset::MaterialInstance* Material;
            aZero::RefCounted<Asset::MaterialType> MaterialRef;
        };

        MeshComponent() = default;

        MeshComponent(Asset::Mesh& mesh, Asset::MaterialInstance& material) {
            this->SetMesh(mesh, material);
        }

        void SetMesh(Asset::Mesh& mesh, Asset::MaterialInstance& material) {
            if (mesh.IsValid() && material.IsValid()) {
                m_Mesh = &mesh;
                const auto& persistentData = mesh.GetPersistentData();
                m_NumSubmeshes = persistentData.Submeshes.size();
                for (uint32_t i = 0; i < persistentData.Submeshes.size(); i++)
                {
                    Submesh newSubmesh;
                    newSubmesh.MetadataIndex = persistentData.SubmeshMetadataSlots[i];
                    newSubmesh.MaterialRef = material.GetReference();
                    m_Submeshes[i] = newSubmesh;
                    this->SetMaterial(i, material);
                }
            }
        }

        void SetMaterial(uint32_t submeshIndex, Asset::MaterialInstance& material) {
            if (material.IsValid() && submeshIndex < m_NumSubmeshes) {
                m_Submeshes[submeshIndex].MaterialPtr = material.GetBDA();
                m_Submeshes[submeshIndex].MaterialTypeIndex = material.GetMaterial()->get().GetRuntimeTypeID();
                m_Submeshes[submeshIndex].Material = &material;
                m_Submeshes[submeshIndex].MaterialRef = material.GetReference();
            }
        }

        std::span<const Submesh> GetSubmeshes() const {
            return { m_Submeshes.data(), m_NumSubmeshes };
        }
    private:

        const Asset::Mesh* m_Mesh = nullptr;
        std::array<Submesh, s_MaxNumberOfSubmeshes> m_Submeshes;
        uint32_t m_NumSubmeshes = 0;
    };
}