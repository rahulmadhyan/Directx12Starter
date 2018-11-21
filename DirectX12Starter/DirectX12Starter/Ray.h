#pragma once
#include "stdafx.h"
#include <DirectXMath.h>

struct Ray
{
	Ray()
	{
		origin = XMFLOAT3(0.0f, 0.0f, 0.0f);
		direction = XMFLOAT3(0.0f, 0.0f, 0.0f);
		distance = 0;
	}

	DirectX::XMFLOAT3 origin;
	DirectX::XMFLOAT3 direction;
	float distance;
};