#pragma once
#include "stdafx.h"
#include <DirectXMath.h>

struct Ray
{
	DirectX::XMFLOAT3 origin;
	DirectX::XMFLOAT3 direction;
	float distance;
};