#pragma once
#include "Entity.h"
#include "Player.h"
class Enemies
{
public:
	Enemies();
	~Enemies();

	void Update(const Timer &timer, Entity* playerEntity, std::vector<Entity*> enemyEntities);

private:
	float moveSpeed = 5.0f;

};

