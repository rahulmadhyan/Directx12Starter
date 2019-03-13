#include "Enemies.h"

Enemies::Enemies(SystemData *systemData) : systemData(systemData)
{

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
}

void Enemies::SetWaypointEntitites(std::vector<Entity*> wayPointEntities)
{
	this->wayPointEntities = wayPointEntities;
}

// have the update function include waypoints vector
void Enemies::Update(const Timer &timer)
{
	const float deltaTime = timer.GetDeltaTime();
	//int currentWaypointIndex = 0;

	XMVECTOR playerPosition = XMLoadFloat3(systemData->GetWorldPosition(playerEntity->SystemWorldIndex));

	for (auto e : enemyEntities)
	{
		XMVECTOR enemyPosition = XMLoadFloat3(systemData->GetWorldPosition(e->SystemWorldIndex));
		XMVECTOR differenceVector = XMVectorSubtract(playerPosition, enemyPosition);
		XMVECTOR length = XMVector3Length(differenceVector);

		const XMFLOAT3* enemyRotation = systemData->GetWorldRotation(e->SystemWorldIndex);
		XMVECTOR enemyRotationVector = XMLoadFloat3(enemyRotation);
		//XMVECTOR forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

		XMVECTOR normalDifferenceVector = XMVector3Normalize(differenceVector);

		XMVECTOR rotation = XMVectorSubtractAngles(differenceVector, enemyRotationVector);

		XMMATRIX rotMatrix = XMMatrixRotationRollPitchYaw(0.0f, enemyRotation->y, 0.0f);

		float distance = 0.0f;
		XMStoreFloat(&distance, length);

		// Waypoint calculation
		Entity* currentWaypoint = wayPointEntities[e->currentWaypointIndex % 4];
		//XMVECTOR currentWaypointVector = XMLoadFloat3(currentWaypoint);

		XMVECTOR currentWaypointVector = XMLoadFloat3(systemData->GetWorldPosition(currentWaypoint->SystemWorldIndex));
		XMVECTOR differenceWaypointVector = XMVectorSubtract(currentWaypointVector, enemyPosition);
		XMVECTOR waypointLength = XMVector3Length(differenceWaypointVector);

		XMVECTOR normaldifferenceWaypointVector = XMVector3Normalize(differenceWaypointVector);

		float distanceToWaypoint = 0.0f;
		XMStoreFloat(&distanceToWaypoint, waypointLength);

		if (e->isRanged && distance < 20.0f)
		{
			// look at
			XMVECTOR newPosition = XMVector3Transform(enemyPosition, rotMatrix);

			XMFLOAT3 rangedEnemyPos;
			XMStoreFloat3(&rangedEnemyPos, newPosition);

			systemData->SetRotation(e->SystemWorldIndex, 0.0f, rangedEnemyPos.y, 0.0f); 			
			systemData->SetWorldMatrix(e->SystemWorldIndex);

			e->NumFramesDirty = gNumberFrameResources;
			
			// fire
		}
		else if(!e->isRanged)
		{
			if (distance < 20.0f && distance > 1.0f)
			{
				systemData->SetTranslation(e->SystemWorldIndex, XMVectorGetX(normalDifferenceVector) * deltaTime * moveSpeed, 0.0f, XMVectorGetZ(normalDifferenceVector) * deltaTime * moveSpeed);
				systemData->SetWorldMatrix(e->SystemWorldIndex);

				e->NumFramesDirty = gNumberFrameResources;

				// fire or attack
			}
			else if(distanceToWaypoint > 1.0f)
			{
				// patrol waypoints
				systemData->SetTranslation(e->SystemWorldIndex, XMVectorGetX(normaldifferenceWaypointVector) * deltaTime * moveSpeed * 0.6f, 0.0f, XMVectorGetZ(normaldifferenceWaypointVector) * deltaTime * moveSpeed * 0.6f);
				systemData->SetWorldMatrix(e->SystemWorldIndex);

				e->NumFramesDirty = gNumberFrameResources;
			}
			else if (distanceToWaypoint < 1.0f)
			{
				e->currentWaypointIndex++;
			}
		}
	}
}