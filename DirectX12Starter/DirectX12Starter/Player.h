#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include "Timer.h"
#include "InputManager.h"
#include "Renderable.h"
using namespace DirectX;

class Player
{
public:
	Player();
	~Player();

	void Update(const Timer &timer, Renderable *playerRenderable);

private:
	float xTranslation;
	float zTranslation;
	float yRotation;
	
	float moveRate;
};

