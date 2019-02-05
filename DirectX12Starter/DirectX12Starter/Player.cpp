#include "Player.h"

extern const int gNumberFrameResources;

Player::Player(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, SystemData *systemData) : systemData(systemData)
{
	xTranslation = 0;
	zTranslation = 0;
	yRotation = 0;

	moveRate = 0.0001f;

	shootingRay = new Ray();
	shootingRay->origin = XMFLOAT3(0.0f, 0.0f, 0.0f);
	shootingRay->direction = XMFLOAT3(0.0f, 0.0f, 1.0f);
	shootingRay->distance = 50.0f;

	emitter = new Emitter(
		device,
		commandList,
		100,
		100,
		5,
		0.1f,
		5.0f,
		XMFLOAT4(1.0f, 0.1, 0.1f, 0.2f),
		XMFLOAT4(1.0f, 0.6f, 0.1f, 0),
		XMFLOAT3(-2.0f, 2.0f, 0.0f),
		XMFLOAT3(2.0f, 2.0f, 0.0f),
		XMFLOAT3(0.0f, -1.0f, 0.0f)
	);
}

Player::~Player()
{
	delete shootingRay;
	delete emitter;
}

const Ray* Player::GetRay() const
{
	return shootingRay;
}

Emitter* Player::GetEmitter() const
{
	return emitter;
}

void Player::Update(const Timer &timer, Entity *playerEntity, std::vector<Entity*> enemyEntities)
{
	UINT playerEntityIndex = playerEntity->SystemWorldIndex;
	const XMFLOAT3* playerRotation = systemData->GetWorldRotation(playerEntity->SystemWorldIndex);

	XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(0.0f, playerRotation->y, 0.0f);

	// move
	{
		const float deltaTime = timer.GetDeltaTime();

		xTranslation = InputManager::getInstance()->getLeftStickX();
		zTranslation = InputManager::getInstance()->getLeftStickY();
		yRotation = InputManager::getInstance()->getRightStickX();

		UINT playerEntityIndex = playerEntity->SystemWorldIndex;
		const XMFLOAT3* playerRotation = systemData->GetWorldRotation(playerEntity->SystemWorldIndex);

		XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(0.0f, playerRotation->y, 0.0f);

		XMVECTOR newPosition = XMVectorSet(xTranslation * deltaTime * moveRate, 0.0f, zTranslation * deltaTime * moveRate, 0.0f);
		newPosition = XMVector3Transform(newPosition, rotationMatrix);

		XMFLOAT3 newPos;
		XMStoreFloat3(&newPos, newPosition);

		systemData->SetTranslation(playerEntityIndex, newPos.x, newPos.y, newPos.z);
		systemData->SetRotation(playerEntityIndex, 0.0f, moveRate * yRotation * deltaTime, 0.0f);
		systemData->SetWorldMatrix(playerEntityIndex);

		playerEntity->NumFramesDirty = gNumberFrameResources;
	}

	emitter->Update(timer.GetDeltaTime());

	// ray-casting
	{
		if (InputManager::getInstance()->isControllerButtonPressed(XINPUT_GAMEPAD_RIGHT_SHOULDER))
		{
			for (auto enemy : enemyEntities)
			{
				shootingRay->origin = *(systemData->GetWorldPosition(playerEntityIndex));
				XMVECTOR rayOrigin = XMLoadFloat3(&(shootingRay->origin));

				XMVECTOR rayDirection = XMLoadFloat3(&(shootingRay->direction));
				rayDirection = XMVector3Transform(rayDirection, rotationMatrix);

				XMMATRIX W = XMLoadFloat4x4(systemData->GetWorldMatrix(enemy->SystemWorldIndex));
				XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(W), W);

				rayOrigin = XMVector3TransformCoord(rayOrigin, invWorld);
				rayDirection = XMVector3TransformNormal(rayDirection, invWorld);

				// Make the ray direction unit length for the intersection tests.
				rayDirection = XMVector3Normalize(rayDirection);

				float rayDistance = shootingRay->distance;

				std::wstringstream out;

				if (enemy->meshData.Bounds.Intersects(rayOrigin, rayDirection, rayDistance))
				{
					out << "TRUE " << std::to_wstring(enemy->SystemWorldIndex) << "\n";
					XMFLOAT3 enemyPosition = *systemData->GetWorldPosition(enemy->SystemWorldIndex);
					emitter->SetEmitterPosition(enemyPosition.x, enemyPosition.y, enemyPosition.z);
				}
				else
				{
					out << "FALSE " << std::to_wstring(enemy->SystemWorldIndex) << "\n";
					emitter->SetEmitterPosition(0.0f, 0.0f, 0.0f);
				}
				OutputDebugStringW(out.str().c_str());
			}
		}
	}

	
}