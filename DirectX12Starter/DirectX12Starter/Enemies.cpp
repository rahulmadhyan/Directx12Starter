#include "Enemies.h"

Enemies::Enemies()
{
}


Enemies::~Enemies()
{
}

void Enemies::Update(Entity* playerEntity, std::vector<Entity*> enemyEntities)
{
	for (size_t i = 0; i < enemyEntities.size(); i++)
	{
		// enemy AI
	}
}