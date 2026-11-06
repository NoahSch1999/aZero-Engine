#pragma once
#include <vector>
#include <array>
#include <string>
#include <map>
#include <unordered_map>
#include <optional>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <type_traits>

namespace aZero::Asset
{
	enum class ParameterType {
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
			sizeof(std::byte) * 1,
			sizeof(std::byte) * 2,
			sizeof(std::byte) * 3,
			sizeof(std::byte) * 4,

			sizeof(uint32_t),
			sizeof(uint32_t),
			sizeof(uint32_t),
			sizeof(uint64_t), // BDA
		};

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
			std::size_t TotalSize;
		};

		std::string m_Name;
		std::map<std::string, Parameter> Parameters;
		PropertyFlags Properties;
		std::filesystem::path MaterialShaderPath;

		const SizeInfo GetParameterSizeInfo() const
		{
			SizeInfo info;
			std::size_t offset = 0;
			for (const auto& [key, value] : Parameters)
			{
				info.Offsets[key] = offset; // todo Handle alignment
				offset += value.GetSize();
			}
			info.TotalSize = offset;
			return info;
		}
	};

	using MaterialRuntimeTypeID = uint32_t;
	using MaterialInstanceRuntimeID = VkDeviceAddress;
	constexpr MaterialInstanceRuntimeID g_InvalidRuntimeID = std::numeric_limits<MaterialInstanceRuntimeID>::max();
	struct MaterialInstance;

	class MaterialType
	{
	public:
		MaterialType(const MaterialDefinition& definition, MaterialRuntimeTypeID runtimeTypeID)
			:m_Definition(definition), m_ParameterSizeInfo(definition.GetParameterSizeInfo()), m_RuntimeTypeID(runtimeTypeID)
		{
		}

		const MaterialDefinition& GetDefinition() const { return m_Definition; }
		const MaterialDefinition::SizeInfo& GetParameterSizeInfo() const { return m_ParameterSizeInfo; }
		MaterialRuntimeTypeID GetRuntimeTypeID() const { return m_RuntimeTypeID; }
	private:
		MaterialDefinition m_Definition;
		MaterialDefinition::SizeInfo m_ParameterSizeInfo;
		MaterialRuntimeTypeID m_RuntimeTypeID;
	};
}
