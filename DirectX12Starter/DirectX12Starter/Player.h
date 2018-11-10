#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include "Timer.h"
#include "InputManager.h"
#include "Entity.h"
using namespace DirectX;

class Player
{
public:
	Player();
	~Player();

	void Update(const Timer &timer, Entity *playerEntity);

private:
	float xTranslation;
	float zTranslation;
	float yRotation;
	
	float moveRate;
};

