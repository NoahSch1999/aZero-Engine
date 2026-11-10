#pragma once
#include <vector>
#include <ktx.h>
#include "misc/UUID.hpp"
#include "render_api/Image.hpp"

namespace aZero::Asset
{
	struct AssetBase
	{
		std::string Name;
		std::array<char, aZero::Helper::UUID::GetSizeAsString()> UUID;
	};
}