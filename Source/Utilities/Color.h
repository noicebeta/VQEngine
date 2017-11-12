//	DX11Renderer - VDemo | DirectX11 Renderer
//	Copyright(C) 2016  - Volkan Ilbeyli
//
//	This program is free software : you can redistribute it and / or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program.If not, see <http://www.gnu.org/licenses/>.
//
//	Contact: volkanilbeyli@gmail.com

#pragma once

#include "utils.h"

#include <map>
#include <vector>
#include <array>

using namespace DirectX;

enum class EColorValue
{
	BLACK = 0,
	WHITE,
	RED,
	GREEN,
	BLUE,
	YELLOW,
	MAGENTA,
	CYAN,
	GRAY,
	LIGHT_GRAY,
	ORANGE,
	PURPLE,
	GOLD,

	COUNT
};


struct LinearColor
{
	using ColorPalette = std::array < const LinearColor, static_cast<int>(EColorValue::COUNT)>;

public:
	LinearColor();
	LinearColor(const vec3&);
	LinearColor(float r, float g, float b);
	LinearColor& operator=(const LinearColor&);
	LinearColor& operator=(const vec3&);

	vec3 Value() const { return value; }
	static const ColorPalette Palette();
	static vec3 RandColorF3();
	static XMVECTOR RandColorV();
	static LinearColor	RandColor();

	operator vec3() const { return value; }

	//static Color GetColorByName(std::string);
	//static std::string GetNameByColor(Color c);

public:
	static const LinearColor black, white, red, green, blue, magenta, yellow, cyan, gray, light_gray, orange, purple, gold;
	static const ColorPalette s_palette;
private:
	vec3 value;
};

