#pragma once
#include "SystemData.h"
#include "ThreadPool.h"
#include "Entity.h"
#include "Player.h"
class Enemies
{
public:
	Enemies(SystemData *systemData);
	~Enemies();

	void Update2(const Timer &timer, Entity* playerEntity, std::vector<EnemyEntity*> enemyEntities, std::vector<Entity*> waypoints);

	void SetPlayerEntity(Entity* playerEntity);
	void SetEnemyEntitites(std::vector<EnemyEntity*> enemyEntities);
	void SetWaypointEntitites(std::vector<Entity*> wayPointEntities);

	void Update(const Timer &timer);
	void UpdateEnemies(std::vector<EnemyUpdateEntity*> enemyEntities);
	void UpdateEnemy(EnemyUpdateEntity* e);

private:
	float moveSpeed = 10.0f;
	float rotationSpeed = 5.0f;
	float deltaTime;

	SystemData* systemData;
	ThreadPool jobSystem1;

	Entity* playerEntity;
	std::vector<EnemyEntity*> enemyEntities;
	std::vector<EnemyUpdateEntity*> enemyUpdateEntities;
	std::vector<std::vector<EnemyUpdateEntity*>> listEnemies;
	
	std::vector<Entity*> wayPointEntities;
};

