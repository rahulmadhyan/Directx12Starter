#pragma once
#include "SystemData.h"
#include "Entity.h"
#include "Player.h"
class Enemies
{
public:
	Enemies(SystemData *systemData);
	~Enemies();

	void SetPlayerEntity(Entity* playerEntity);
	void SetEnemyEntitites(std::vector<EnemyEntity*> enemyEntities);
	void SetWaypointEntitites(std::vector<Entity*> wayPointEntities);

	void Update(const Timer &timer);

private:
	float moveSpeed = 1.0f;
	float rotationSpeed = 5.0f;

	SystemData* systemData;
	Entity* playerEntity;
	std::vector<EnemyEntity*> enemyEntities;
	std::vector<Entity*> wayPointEntities;
};

