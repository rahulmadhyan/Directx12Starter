#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include "Timer.h"
#include "InputManager.h"
#include "Entity.h"
#include "SystemData.h"

using namespace DirectX;

class Player
{
public:
	Player(SystemData *systemData);
	~Player();

	void Update(const Timer &timer, Entity *playerEntity);

private:
	SystemData* systemData;

	float xTranslation;
	float zTranslation;
	float yRotation;
	
	float moveRate;
};

