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
void Enemies::Update(const Timer &timer, Entity* playerEntity, std::vector<Entity*> enemyEntities)
{
	const float deltaTime = timer.GetDeltaTime();

	XMVECTOR playerPosition = XMLoadFloat3(systemData->GetWorldPosition(playerEntity->SystemWorldIndex));

	for (auto e : enemyEntities)
	{
		XMVECTOR enemyPosition = XMLoadFloat3(systemData->GetWorldPosition(e->SystemWorldIndex));
		XMVECTOR differenceVector = XMVectorSubtract(playerPosition, enemyPosition);
		XMVECTOR length = XMVector3Length(differenceVector);

		XMVECTOR normalDifferenceVector = XMVector3Normalize(differenceVector);

		float distance = 0.0f;
		XMStoreFloat(&distance, length);

		if (distance < 20.0f && distance > 5.0f)
		{
			systemData->SetTranslation(e->SystemWorldIndex, XMVectorGetX(normalDifferenceVector) * deltaTime * moveSpeed, 0.0f, XMVectorGetZ(normalDifferenceVector) * deltaTime * moveSpeed);
			systemData->SetWorldMatrix(e->SystemWorldIndex);

			e->NumFramesDirty = gNumberFrameResources;
		}

		//if (e->isRanged && distance < 20.0f)
		//{
		//	// look at

		//	// fire
		//}
		//else
		//{
		//	if (distance < 20.0f && distance > 5.0f)
		//	{
		//		systemData->SetTranslation(e->SystemWorldIndex, XMVectorGetX(normalDifferenceVector) * deltaTime * moveSpeed, 0.0f, XMVectorGetZ(normalDifferenceVector) * deltaTime * moveSpeed);
		//		systemData->SetWorldMatrix(e->SystemWorldIndex);

		//		e->NumFramesDirty = gNumberFrameResources;
		//	}
		//	else
		//	{
		//		//patrol waypoints.
		//	}
		//}
	}
}