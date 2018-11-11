#pragma once
#include "Entity.h"
class Enemies
{
public:
	Enemies();
	~Enemies();

	void Update(Entity* playerEntity, std::vector<Entity*> enemyEntities);
};

