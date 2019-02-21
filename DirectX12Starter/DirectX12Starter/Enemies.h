#pragma once
#include "SystemData.h"
#include "Entity.h"
#include "Player.h"
class Enemies
{
public:
	Enemies();
	Enemies(SystemData *systemData);
	~Enemies();

	void Update(const Timer &timer, Entity* playerEntity, std::vector<Entity*> enemyEntities);

private:
	float moveSpeed = 5.0f;
	float rotationSpeed = 5.0f;

	SystemData* systemData;
};

