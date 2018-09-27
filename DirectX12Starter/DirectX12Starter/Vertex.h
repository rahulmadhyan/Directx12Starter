#pragma once

#include <DirectXMath.h>

struct Vertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT4 Color;

	Vertex(float x, float y, float z, float r, float g, float b, float a) : Position(x, y, z), Color(r, g, b, a)
	{
	}
};