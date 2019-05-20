#include "Enemies.h"

Enemies::Enemies(SystemData *systemData) : systemData(systemData)
{
	const unsigned int threadCount = std::thread::hardware_concurrency();
	//jobSystem1.Start(threadCount);
}

Enemies::~Enemies()
{

}

void Enemies::SetPlayerEntity(Entity* playerEntity)
{
	this->playerEntity = playerEntity;
}

void Enemies::SetEnemyEntitites(std::vector<EnemyEntity*> enemyEntities)
{
	this->enemyEntities = enemyEntities;

	for (auto e : enemyEntities)
	{
		this->enemyUpdateEntities.emplace_back(new EnemyUpdateEntity(e->SystemWorldIndex, e->currentWaypointIndex));
	}

	size_t threadCount = std::thread::hardware_concurrency();
	size_t enemiesPerThread = enemyUpdateEntities.size() / threadCount;

	for (size_t i = 0; i < threadCount; i++)
	{
		auto start_itr = std::next(enemyUpdateEntities.begin(), i * enemiesPerThread);
		auto end_itr = std::next(enemyUpdateEntities.begin(), (i * enemiesPerThread) + enemiesPerThread);
		
		std::vector<EnemyUpdateEntity*> currentEnemies(enemiesPerThread);

		std::copy(start_itr, end_itr, currentEnemies.begin());

		listEnemies.push_back(currentEnemies);
	}
}

void Enemies::SetWaypointEntitites(std::vector<Entity*> wayPointEntities)
{
	this->wayPointEntities = wayPointEntities;
}

// have the update function include waypoints vector
void Enemies::Update(const Timer &timer)
{
	// Change the type of update function here
	// To change check different behavior like data oriented and object oriented

	deltaTime = timer.GetDeltaTime();

	for (auto e : enemyUpdateEntities)
	{
		UpdateEnemy(e);
	}

	//Update2();

	/*for (size_t i = 0; i < listEnemies.size(); i++)
	{
		jobSystem1.Submit([=] {
			UpdateEnemies(std::ref(listEnemies[i]));
		});
	}*/

	for (auto e : enemyEntities)
	{
		e->NumFramesDirty = gNumberFrameResources;
	}
}

void Enemies::UpdateEnemies(std::vector<EnemyUpdateEntity*> enemyEntities)
{
	for (auto e : enemyEntities)
	{
		UpdateEnemy(e);
	}

	//Update2();
}

// Example for enemy Update with object oriented pattern.
void Enemies::UpdateEnemy(EnemyUpdateEntity* e)
{
	XMVECTOR playerPosition = XMLoadFloat3(systemData->GetWorldPosition(playerEntity->SystemWorldIndex));
	XMVECTOR enemyPosition = XMLoadFloat3(systemData->GetWorldPosition(e->SystemWorldIndex));
	XMVECTOR differenceVector = XMVectorSubtract(playerPosition, enemyPosition);
	XMVECTOR length = XMVector3Length(differenceVector);

	const XMFLOAT3* enemyRotation = systemData->GetWorldRotation(e->SystemWorldIndex);
	XMVECTOR enemyRotationVector = XMLoadFloat3(enemyRotation);

	XMVECTOR normalDifferenceVector = XMVector3Normalize(differenceVector);

	XMVECTOR rotation = XMVectorSubtractAngles(differenceVector, enemyRotationVector);

	XMMATRIX rotMatrix = XMMatrixRotationRollPitchYaw(0.0f, enemyRotation->y, 0.0f);

	float distance = 0.0f;
	XMStoreFloat(&distance, length);

	// Waypoint calculation
	Entity* currentWaypoint = wayPointEntities[e->CurrentWaypointIndex % 4];

	XMVECTOR currentWaypointVector = XMLoadFloat3(systemData->GetWorldPosition(currentWaypoint->SystemWorldIndex));
	XMVECTOR differenceWaypointVector = XMVectorSubtract(currentWaypointVector, enemyPosition);
	XMVECTOR waypointLength = XMVector3Length(differenceWaypointVector);

	XMVECTOR normaldifferenceWaypointVector = XMVector3Normalize(differenceWaypointVector);

	float distanceToWaypoint = 0.0f;
	XMStoreFloat(&distanceToWaypoint, waypointLength);

	
	if (distanceToWaypoint > 3.0f)
	{
		// patrol waypoints

		systemData->SetTranslation(e->SystemWorldIndex, XMVectorGetX(normaldifferenceWaypointVector) * deltaTime * moveSpeed * 0.6f, 0.0f, XMVectorGetZ(normaldifferenceWaypointVector) * deltaTime * moveSpeed * 0.6f);
		systemData->SetWorldMatrix(e->SystemWorldIndex);

	}
	else if (distanceToWaypoint < 3.0f)
	{
		e->CurrentWaypointIndex++;
	}
}

// Update function using Data oriented pattern
void Enemies::Update2()
{
	XMVECTOR playerPosition = XMLoadFloat3(systemData->GetWorldPosition(playerEntity->SystemWorldIndex));

	for (auto e : enemyEntities)
	{
		XMVECTOR enemyPosition = XMLoadFloat3(systemData->GetWorldPosition(e->SystemWorldIndex));
		XMVECTOR differenceVector = XMVectorSubtract(playerPosition, enemyPosition);
		XMVECTOR length = XMVector3Length(differenceVector);

		XMVECTOR normalDifferenceVector = XMVector3Normalize(differenceVector);

		XMStoreFloat3(&e->diffVectorNormal, normalDifferenceVector);
		XMStoreFloat(&e->distanceToPlayer, length);
	}

	for (auto e : enemyEntities)
	{
		Entity* currentWaypoint = wayPointEntities[e->currentWaypointIndex % 4];
		
		XMVECTOR enemyPosition = XMLoadFloat3(systemData->GetWorldPosition(e->SystemWorldIndex));
		XMVECTOR currentWaypointVector = XMLoadFloat3(systemData->GetWorldPosition(currentWaypoint->SystemWorldIndex));
		XMVECTOR differenceWaypointVector = XMVectorSubtract(currentWaypointVector, enemyPosition);
		XMVECTOR waypointLength = XMVector3Length(differenceWaypointVector);

		XMVECTOR normaldifferenceWaypointVector = XMVector3Normalize(differenceWaypointVector);

		XMStoreFloat3(&e->diffWayPointVecNormal, normaldifferenceWaypointVector);
		XMStoreFloat(&e->distanceToWaypoint, waypointLength);
	}

	for (auto e : enemyEntities)
	{
		XMVECTOR normalDifferenceVector = XMLoadFloat3(&e->diffVectorNormal);
		XMVECTOR normaldifferenceWaypointVector = XMLoadFloat3(&e->diffWayPointVecNormal);

		if (!e->isRanged)
		{
			if (e->distanceToPlayer < 20.0f && e->distanceToPlayer > 1.0f)
			{
				systemData->SetTranslation(e->SystemWorldIndex, XMVectorGetX(normalDifferenceVector) * deltaTime * moveSpeed, 0.0f, XMVectorGetZ(normalDifferenceVector) * deltaTime * moveSpeed);
				systemData->SetWorldMatrix(e->SystemWorldIndex);

				e->NumFramesDirty = gNumberFrameResources;
			}
			else if (e->distanceToWaypoint > 1.0f)
			{
				// patrol waypoints
				systemData->SetTranslation(e->SystemWorldIndex, XMVectorGetX(normaldifferenceWaypointVector) * deltaTime * moveSpeed * 0.6f, 0.0f, XMVectorGetZ(normaldifferenceWaypointVector) * deltaTime * moveSpeed * 0.6f);
				systemData->SetWorldMatrix(e->SystemWorldIndex);

				e->NumFramesDirty = gNumberFrameResources;
			}
			else if (e->distanceToWaypoint < 1.0f)
			{
				e->currentWaypointIndex++;
			}
		}
	}

	for (auto e : enemyEntities)
	{
		e->NumFramesDirty = gNumberFrameResources;
	}
}