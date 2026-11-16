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

        MeshComponent() = default;

        MeshComponent(Asset::Mesh& mesh, Asset::MaterialInstance& material) {
            this->SetMesh(mesh, material);
        }

        void SetMesh(Asset::Mesh& mesh, Asset::MaterialInstance& material) {
            if (mesh.IsValid() && material.IsValid()) {
                m_Mesh = &mesh;
                const auto& submeshes = mesh.GetSubmeshes();
                m_NumSubmeshes = submeshes.size();
                for (uint32_t i = 0; i < submeshes.size(); i++)
                {
                    Submesh newSubmesh;
                    newSubmesh.MeshletCount = submeshes[i].MeshletCount;
                    newSubmesh.MeshletBufferPtr = mesh.GetMeshletGpuPtr() + submeshes[i].MeshletOffset * sizeof(Asset::Meshlet);
                    newSubmesh.VertexBufferPtr = mesh.GetVertexGpuPtr();
                    newSubmesh.MeshletBoundsBufferPtr = mesh.GetMeshletBoundsGpuPtr() + submeshes[i].MeshletOffset * sizeof(BoundingVolume::BoundingSphere);
                    newSubmesh.Bounds = submeshes[i].Bounds;
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
            }
        }

        struct Submesh
        {
            aZero::BoundingVolume::BoundingSphere Bounds;
            VkDeviceAddress MeshletBufferPtr, VertexBufferPtr, MeshletBoundsBufferPtr, MaterialPtr;
            uint32_t MeshletCount, MaterialTypeIndex;
            const Asset::MaterialInstance* Material;
        };

        const Asset::Mesh* m_Mesh = nullptr;
        std::array<Submesh, s_MaxNumberOfSubmeshes> m_Submeshes;
        uint32_t m_NumSubmeshes = 0;

    private:
    };
}