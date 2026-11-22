#pragma once
#include "MaterialType.hpp"
#include "misc/Refcounted.hpp"

namespace aZero::System
{
	class MaterialSystem;
}

namespace aZero::Asset
{
	using MaterialInstanceBDA = VkDeviceAddress;
	constexpr MaterialInstanceBDA g_InvalidInstanceBDA = std::numeric_limits<MaterialInstanceBDA>::max();
	class MaterialInstance
	{
		friend class aZero::System::MaterialSystem;
	public:
		MaterialInstance() = default;
		MaterialInstance(MaterialType& diMaterial, const std::string& name, MaterialInstanceBDA instanceBDA, System::MaterialSystem& diMaterialSystem);

		template<typename T> requires std::is_trivially_copyable_v<T>
		void SetParameter(const std::string& name, const T& data, bool markDirty = true)
		{
			if (!m_diMaterial)
			{
				return;
			}

			if (auto param = m_diMaterial->GetDefinition().FindParameter(name))
			{
				const std::size_t paramSize = param->GetSize();
				if (sizeof(data) <= paramSize)
				{
					if (auto iter2 = m_diMaterial->GetParameterSizeInfo().Offsets.find(name); iter2 != m_diMaterial->GetParameterSizeInfo().Offsets.end())
					{
						memcpy(m_Data.data() + iter2->second, &data, sizeof(data));
						this->SetDirty();
					}
				}
			}
		}

		// Spooky scary copies
		void RawCopy(void* ptr, size_t size, bool markDirty = true)
		{
			if (size == m_diMaterial->GetParameterSizeInfo().TotalSize)
			{
				memcpy(m_Data.data(), ptr, size);
				this->SetDirty();
			}
			else
			{
				std::cout << "Error matching material raw copy: " << m_Name << "\n";
			}
		}

		template<typename T> requires std::is_trivially_copyable_v<T>
		std::optional<T> GetParameter(const std::string& name)
		{
			if (!m_diMaterial)
			{
				return {};
			}

			if (auto param = m_diMaterial->GetDefinition().FindParameter(name))
			{
				if (param->GetSize() == sizeof(T))
				{
					if (auto iter2 = m_diMaterial->GetParameterSizeInfo().Offsets.find(name); iter2 != m_diMaterial->GetParameterSizeInfo().Offsets.end())
					{
						T data;
						std::memcpy(&data, m_Data.data() + iter2->second, sizeof(data));
						return data;
					}
				}
			}

			return {};
		}

		bool IsValid() const { return m_diMaterial != nullptr; }
		std::optional<std::reference_wrapper<const MaterialType>> GetMaterial() const
		{
			if (!this->IsValid()) {
				return std::nullopt;
			}
			return *m_diMaterial;
		}

		std::string GetName() const { return m_Name; }

		MaterialInstanceBDA GetBDA() const { return m_InstanceBDA; }

		aZero::RefCounted<Asset::MaterialType> GetReference()
		{
			return aZero::RefCounted<Asset::MaterialType>(*m_diMaterial);
		}

		void SetDirty();
	private:
		MaterialType* m_diMaterial = nullptr;
		aZero::System::MaterialSystem* m_diMaterialSystem = nullptr;
		std::vector<std::byte> m_Data;
		MaterialInstanceBDA m_InstanceBDA = g_InvalidInstanceBDA;
		std::string m_Name;
	};
}
