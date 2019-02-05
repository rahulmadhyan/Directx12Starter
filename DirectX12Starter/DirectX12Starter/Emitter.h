#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include "d3dUtil.h"
#include "FrameResource.h"
#include "Entity.h"

using namespace DirectX;

struct Particle
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT4 Color;
	DirectX::XMFLOAT3 StartVelocity;
	float Size;
	float Age;
};

class Emitter
{
public:
	Emitter(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList,
		int maxParticles,
		int particlePerSecond,
		float lifetime,
		float startSize,
		float endSize,
		DirectX::XMFLOAT4 startColor,
		DirectX::XMFLOAT4 endColor,
		DirectX::XMFLOAT3 startVelocity,
		DirectX::XMFLOAT3 emitterPosition,
		DirectX::XMFLOAT3 emitterAcceleration
		);
	~Emitter();

	int GetMaxParticles();
	ParticleVertex* GetParticleVertices();
	uint16_t* GetParticleIndices();

	DirectX::XMFLOAT3 GetEmitterPosition();
	void SetEmitterPosition(float x, float y, float z);

	void Update(float deltaTime);

private:
	int particlesPerSecond;
	float secondsPerParticle;
	float timeSinceEmit;

	int livingParticleCount;
	float lifetime;

	DirectX::XMFLOAT3 emitterAcceleration;
	DirectX::XMFLOAT3 emitterPosition;
	DirectX::XMFLOAT3 startVelocity;
	DirectX::XMFLOAT4 startColor;
	DirectX::XMFLOAT4 endColor;
	float startSize;
	float endSize;

	ID3D12Device* device;
	ID3D12GraphicsCommandList* commandList;

	Particle* particles;
	int maxParticles;
	int firstDeadIndex;
	int firstAliveIndex;

	ParticleVertex* localParticleVertices;
	uint16_t* indices;

	void UpdateSingleParticle(float deltaTime, int index);
	void SpawnParticle();

	void CopyParticlesToGPU();
	void CopyOneParticle(int index);
};

