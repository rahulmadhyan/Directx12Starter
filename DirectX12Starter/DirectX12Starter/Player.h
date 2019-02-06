#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include "Timer.h"
#include "SystemData.h"
#include "InputManager.h"
#include "Entity.h"
#include "Ray.h"
#include "Emitter.h"
#include <string>

using namespace DirectX;

class Player
{
public:
	Player(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, SystemData *systemData);
	~Player();

	const Ray* GetRay() const;
	Emitter* GetEmitter() const;
	void Update(const Timer &timer, Entity *playerEntity, std::vector<Entity*> enemyEntities);

private:
	float xTranslation;
	float zTranslation;
	float yRotation;
	
	float moveRate;

	SystemData* systemData;

	Ray* shootingRay;

	Emitter* emitter;
};

