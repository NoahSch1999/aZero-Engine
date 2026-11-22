#pragma once
#include <vector>
#include <array>
#include <string>
#include <unordered_map>
#include <optional>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <filesystem>
#include "render_api/Descriptor.hpp"
#include "asset/AssetData.hpp"

namespace aZero::System { class MaterialSystem; }
namespace aZero
{
	template<typename T>
	class RefCounted;
}
namespace aZero::Asset
{
	enum class ParameterType : std::uint32_t {
		float1,
		float2,
		float3,
		float4,
		int1,
		int2,
		int3,
		int4,
		bool1,
		bool2,
		bool3,
		bool4,

		tex1D,
		tex2D,
		tex3D,
		buffer,
		MAX
	};

	enum PropertyFlagBits {
		NoDepth = 1 << 0,
		Transparency = 1 << 1,
		Unlit = 1 << 2
	};
	using PropertyFlags = std::uint32_t;

	struct Parameter
	{
		// todo Change to glm and vk bda etc
		static inline constexpr std::array<std::size_t, static_cast<std::size_t>(ParameterType::MAX)> g_ParameterSizeLUT =
		{
			sizeof(float) * 1,
			sizeof(float) * 2,
			sizeof(float) * 3,
			sizeof(float) * 4,
			sizeof(int32_t) * 1,
			sizeof(int32_t) * 2,
			sizeof(int32_t) * 3,
			sizeof(int32_t) * 4,
			sizeof(int32_t) * 1, // Bools are 4 bytes in slang
			sizeof(int32_t) * 2, // Bools are 4 bytes in slang
			sizeof(int32_t) * 3, // Bools are 4 bytes in slang
			sizeof(int32_t) * 4, // Bools are 4 bytes in slang

			sizeof(RenderAPI::DescriptorIndex),
			sizeof(RenderAPI::DescriptorIndex),
			sizeof(RenderAPI::DescriptorIndex),
			sizeof(VkDeviceAddress)
		};

		constexpr std::size_t GetAlignment() const
		{
			return Type == ParameterType::buffer ? 8 : 4; // Everything else is 4-byte scalars or 32-bit descriptor indices
		}

		ParameterType Type;
		constexpr std::size_t GetSize() const
		{
			return g_ParameterSizeLUT[static_cast<std::size_t>(Type)];
		}

		static constexpr std::size_t GetSize(ParameterType type)
		{
			return g_ParameterSizeLUT[static_cast<std::size_t>(type)];
		}

		Parameter() = default;
		constexpr Parameter(ParameterType type) : Type(type) {}

		constexpr operator ParameterType() const { return Type; }

		constexpr bool operator==(ParameterType a) const { return Type == a; }
		constexpr bool operator!=(ParameterType a) const { return Type != a; }
	};

	struct MaterialDefinition
	{
		struct SizeInfo
		{
			std::unordered_map<std::string, std::size_t> Offsets;
			std::size_t TotalSize = 0;
			std::size_t Alignment = 1;
		};
		std::vector<std::pair<std::string, Parameter>> Parameters;
		PropertyFlags Properties;
		std::filesystem::path MaterialShaderPath;
		std::string MaterialShaderName;

		const Parameter* FindParameter(const std::string& name) const
		{
			for (const auto& [key, value] : Parameters)
			{
				if (key == name)
				{
					return &value;
				}
			}
			return nullptr;
		}

		const SizeInfo GetParameterSizeInfo() const
		{
			// Rounds v up to a multiple of a and returns v unchanged if its already a multiple.
			/*
			*ex: a = 8
					a        = 0b00001000
					a - 1    = 0b00000111   <- mask of the low bits
					~(a - 1) = 0b...11111000   <- clears the low 3 bits
			*/
			auto alignUp = [](std::size_t v, std::size_t a) { return (v + a - 1) & ~(a - 1); };

			SizeInfo info;
			std::size_t offset = 0;
			for (const auto& [key, value] : Parameters)
			{
				const std::size_t align = value.GetAlignment();
				offset = alignUp(offset, align);
				info.Offsets[key] = offset;
				offset += value.GetSize();
				info.Alignment = std::max(info.Alignment, align);
			}
			info.TotalSize = alignUp(offset, info.Alignment);
			return info;
		}

		bool IsValid() const
		{
			return std::filesystem::exists(MaterialShaderPath) && !MaterialShaderName.empty();
		}
	};

	using MaterialRuntimeTypeID = uint32_t;
	class MaterialInstance;

	class MaterialType : public Asset::AssetBase
	{
		friend class aZero::System::MaterialSystem;
		friend class RefCounted<MaterialType>;
	public:
		MaterialType(const MaterialDefinition& definition, MaterialRuntimeTypeID runtimeTypeID)
			:m_Definition(definition), m_ParameterSizeInfo(definition.GetParameterSizeInfo()), m_RuntimeTypeID(runtimeTypeID)
		{
		}

		const MaterialDefinition& GetDefinition() const { return m_Definition; }
		const MaterialDefinition::SizeInfo& GetParameterSizeInfo() const { return m_ParameterSizeInfo; }
		MaterialRuntimeTypeID GetRuntimeTypeID() const { return m_RuntimeTypeID; }
	private:
		void AddRef()
		{
			m_Refcount++;
		}

		void RemoveRef()
		{
			if (m_Refcount > 0)
			{
				m_Refcount--;
			}
		}
		MaterialDefinition m_Definition;
		MaterialDefinition::SizeInfo m_ParameterSizeInfo;
		MaterialRuntimeTypeID m_RuntimeTypeID;
		uint32_t m_Refcount = 0;
	};
}
