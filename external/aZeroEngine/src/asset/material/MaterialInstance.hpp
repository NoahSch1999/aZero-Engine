#pragma once
#include "MaterialType.hpp"

namespace aZero::Asset
{
	using MaterialInstanceBDA = VkDeviceAddress;
	constexpr MaterialInstanceBDA g_InvalidInstanceBDA = std::numeric_limits<MaterialInstanceBDA>::max();
	class MaterialInstance
	{
		friend class aZero::System::MaterialSystem;
	public:
		MaterialInstance() = default;
		MaterialInstance(const MaterialType& diMaterial, const std::string& name, MaterialInstanceBDA instanceBDA)
			:m_Name(name), m_diMaterial(&diMaterial), m_InstanceBDA(instanceBDA)
		{
			m_Data.resize(diMaterial.GetParameterSizeInfo().TotalSize);
		}

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
						m_IsDirty = markDirty;
					}
				}
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

		void SetNotDirty() { m_IsDirty = false; }
		bool IsDirty() const { return m_IsDirty; }
		MaterialInstanceBDA GetBDA() const { return m_InstanceBDA; }
	private:
		const MaterialType* m_diMaterial = nullptr;
		std::vector<std::byte> m_Data;
		MaterialInstanceBDA m_InstanceBDA = g_InvalidInstanceBDA;
		std::string m_Name;
		bool m_IsDirty = true;
	};
}
