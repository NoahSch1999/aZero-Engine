#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL 
#include <glm/gtx/norm.hpp>
#include <numbers>

namespace aZero::Helpers
{
	inline uint16_t float_to_half(float f)
	{
		uint32_t x;
		std::memcpy(&x, &f, sizeof(x));

		uint32_t sign = (x >> 16) & 0x8000;
		int32_t exp = ((x >> 23) & 0xFF) - 127 + 15;
		uint32_t mant = x & 0x7FFFFF;

		if (exp <= 0)
		{
			if (exp < -10)
				return (uint16_t)sign;

			mant = (mant | 0x800000) >> (1 - exp);
			return (uint16_t)(sign | ((mant + 0x1000) >> 13));
		}
		else if (exp >= 31)
		{
			if (mant == 0)
				return (uint16_t)(sign | 0x7C00); // Infinity

			return (uint16_t)(sign | 0x7C00 | (mant >> 13)); // NaN
		}

		return (uint16_t)(sign | (exp << 10) | ((mant + 0x1000) >> 13));
	}

	struct Rectangle
	{
		int32_t TopX; int32_t TopY;
		int32_t Width; int32_t Height;

		Rectangle() = default;
		Rectangle(int32_t topX, int32_t topY, int32_t width, int32_t height)
			:TopX(topX), TopY(topY), Width(width), Height(height) {
		}
	};

	template<typename ValueType>
	std::string HandleNameCollision(const std::string& name, const std::unordered_map<std::string, ValueType>& map)
	{
		std::string tempName;
		while (map.count(name) > 0)
		{
			uint32_t incr = 0;
			tempName = name + "_" + std::to_string(incr);
			incr++;
		}

		if (tempName.length() > 0)
		{
			return tempName;
		}

		return name;
	}

	inline std::string GetPathSuffix(const std::string& filePath)
	{
		const size_t lastDot = filePath.find_last_of('.');
		return filePath.substr(lastDot + 1, filePath.length() - (filePath.length() - lastDot));
	}

	inline std::string GetFilenameFromPath(const std::string& filePath)
	{
		const size_t lastSlash = filePath.find_last_of('/');
		if (lastSlash != std::wstring::npos)
		{
			return filePath.substr(lastSlash + 1, filePath.length() - lastSlash);
		}
		return "";
	}

	inline std::string StripSuffixFromFilePath(const std::string& filePath)
	{
		const size_t lastDot = filePath.find_last_of('.');
		return filePath.substr(0, lastDot);
	}

	inline uint32_t Pack8To32(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
	{
		return
			(static_cast<uint32_t>(d) << 24) |
			(static_cast<uint32_t>(c) << 16) |
			(static_cast<uint32_t>(b) << 8) |
			(static_cast<uint32_t>(a));
	}

	inline std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> Unpack32To8(uint32_t value32Bit)
	{
		return std::make_tuple(static_cast<uint8_t>(value32Bit & 0xFFFF), static_cast<uint8_t>((value32Bit >> 8) & 0xFFFF), static_cast<uint8_t>((value32Bit >> 16) & 0xFFFF), static_cast<uint8_t>((value32Bit >> 24) & 0xFFFF));
	}

	inline uint32_t Pack16To32(uint16_t low, uint16_t high) {
		return (static_cast<uint32_t>(high) << 16) | low;
	}

	inline std::tuple<uint16_t, uint16_t> Unpack32To16(uint32_t value32Bit)
	{
		return std::make_tuple(static_cast<uint16_t>(value32Bit & 0xFFFF), static_cast<uint16_t>((value32Bit >> 16) & 0xFFFF));
	}

	inline glm::vec2 EncodeNormalOctahedral(const glm::vec3& n)
	{
		float invL1 = 1.0f / (std::fabs(n.x) + std::fabs(n.y) + std::fabs(n.z));
		float x = n.x * invL1;
		float y = n.y * invL1;
		float z = n.z * invL1;

		glm::vec2 enc = { x, y };

		if (z < 0.0f)
		{
			float oldX = enc.x;
			float oldY = enc.y;

			enc.x = (1.0f - std::fabs(oldY)) * (oldX >= 0.0f ? 1.0f : -1.0f);
			enc.y = (1.0f - std::fabs(oldX)) * (oldY >= 0.0f ? 1.0f : -1.0f);
		}

		return enc;
	}
}