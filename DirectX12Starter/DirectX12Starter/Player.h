#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include "Timer.h"
#include "SystemData.h"
#include "InputManager.h"
#include "Entity.h"
#include "Ray.h"
#include <string>
using namespace DirectX;

class Player
{
public:
	Player(SystemData *systemData);
	~Player();

	void Update(const Timer &timer, Entity *playerEntity, std::vector<Entity*> enemyEntities);

private:
	float xTranslation;
	float zTranslation;
	float yRotation;
	
	float moveRate;

	Ray shootingRay;

	SystemData* systemData;
};

