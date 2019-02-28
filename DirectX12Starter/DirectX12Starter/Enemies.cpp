#include "Enemies.h"

Enemies::Enemies()
{
}

Enemies::Enemies(SystemData *systemData) : systemData(systemData)
{

}

Enemies::~Enemies()
{
}

// have the update function include waypoints vector
void Enemies::Update(const Timer &timer, Entity* playerEntity, std::vector<EnemyEntity*> enemyEntities, std::vector<XMFLOAT3*> waypoints)
{
	const float deltaTime = timer.GetDeltaTime();
	//int currentWaypointIndex = 0;

	XMVECTOR playerPosition = XMLoadFloat3(systemData->GetWorldPosition(playerEntity->SystemWorldIndex));

	for (auto e : enemyEntities)
	{
		XMVECTOR enemyPosition = XMLoadFloat3(systemData->GetWorldPosition(e->SystemWorldIndex));
		XMVECTOR differenceVector = XMVectorSubtract(playerPosition, enemyPosition);
		XMVECTOR length = XMVector3Length(differenceVector);

		XMVECTOR forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

		XMVECTOR normalDifferenceVector = XMVector3Normalize(differenceVector);

		XMVECTOR rotation = XMVectorSubtractAngles(differenceVector, forward);

		XMMATRIX rotMatrix = XMMatrixRotationRollPitchYawFromVector(rotation);

		float distance = 0.0f;
		XMStoreFloat(&distance, length);

		// Waypoint calculation
		XMFLOAT3* currentWaypoint = waypoints[currentWaypointIndex % 3];
		XMVECTOR currentWaypointVector = XMLoadFloat3(currentWaypoint);
		XMVECTOR differenceWaypointVector = XMVectorSubtract(currentWaypointVector, enemyPosition);
		XMVECTOR waypointLength = XMVector3Length(differenceWaypointVector);

		XMVECTOR normaldifferenceWaypointVector = XMVector3Normalize(differenceWaypointVector);

		float distanceToWaypoint = 0.0f;
		XMStoreFloat(&distanceToWaypoint, waypointLength);

		if (e->isRanged && distance < 20.0f)
		{
			// look at
			enemyPosition = XMVector3Transform(enemyPosition, rotMatrix);

			XMFLOAT3 rangedEnemyPos;
			XMStoreFloat3(&rangedEnemyPos, enemyPosition);

			systemData->SetTranslation(e->SystemWorldIndex, rangedEnemyPos.x, rangedEnemyPos.y, rangedEnemyPos.z);
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
				currentWaypointIndex++;
			}
		}
	}
}