#pragma once

#include <DirectXMath.h>

struct Vertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT2 UV;

	Vertex(float x, float y, float z, float u, float v) : Position(x, y, z), UV(u, v)
	{
	}
};