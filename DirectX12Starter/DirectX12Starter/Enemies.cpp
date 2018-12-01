#include "Enemies.h"

Enemies::Enemies()
{
}


Enemies::~Enemies()
{
}

void Enemies::Update(const Timer &timer, Entity* playerEntity, std::vector<Entity*> enemyEntities)
{
	const float deltaTime = timer.GetDeltaTime();

	XMVECTOR playerPosition = XMLoadFloat3(&playerEntity->Position);

	for (size_t i = 0; i < enemyEntities.size(); i++)
	{
		XMVECTOR enemyPosition = XMLoadFloat3(&enemyEntities[i]->Position);
		XMVECTOR differenceVector = XMVectorSubtract(playerPosition, enemyPosition);
		XMVECTOR length = XMVector3Length(differenceVector);

		XMVECTOR normalDifferenceVector = XMVector3Normalize(differenceVector);

		float distance = 0.0f;
		XMStoreFloat(&distance, length);

		if (distance < 20.0f && distance > 5.0f)
		{
			enemyEntities[i]->SetTranslation(XMVectorGetX(normalDifferenceVector) * deltaTime * moveSpeed, 0.0f, XMVectorGetZ(normalDifferenceVector) * deltaTime * moveSpeed);
			enemyEntities[i]->SetWorldMatrix();

			enemyEntities[i]->NumFramesDirty = gNumberFrameResources;
		}
	}
}