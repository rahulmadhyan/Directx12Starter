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

	void Update2();

	void SetPlayerEntity(Entity* playerEntity);
	void SetEnemyEntitites(std::vector<EnemyEntity*> enemyEntities);
	void SetWaypointEntitites(std::vector<Entity*> wayPointEntities);

	//void SetEnemyPositions(std::vector<EnemyEntity*> enemyEntities);

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
	
	//std::vector<XMFLOAT3> enemyPositions;
	std::vector<XMFLOAT3> wayPointPositions;

	std::vector<Entity*> wayPointEntities;
};

