#include "Player.h"

extern const int gNumberFrameResources;

Player::Player()
{
	xTranslation = 0;
	zTranslation = 0;
	yRotation = 0;

	moveRate = 0.0001f;
}

Player::~Player()
{
}

void Player::Update(const Timer &timer, Entity *playerEntity)
{
	const float deltaTime = timer.GetDeltaTime();

	xTranslation = InputManager::getInstance()->getLeftStickX();
	zTranslation = InputManager::getInstance()->getLeftStickY();
	yRotation = InputManager::getInstance()->getRightStickX();

	XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(0.0f, playerEntity->Rotation.y, 0.0f);
	
	XMVECTOR newPosition = XMVectorSet(xTranslation * deltaTime * moveRate, 0.0f, zTranslation * deltaTime * moveRate, 0.0f);
	newPosition = XMVector3Transform(newPosition, rotationMatrix);

	XMFLOAT3 newPos;
	XMStoreFloat3(&newPos, newPosition);

	playerEntity->SetTranslation(newPos.x, newPos.y, newPos.z);
	playerEntity->SetRotation(0.0f, moveRate * yRotation * deltaTime, 0.0f);
	playerEntity->SetWorldMatrix();

	playerEntity->NumFramesDirty = gNumberFrameResources;
}