#pragma once
#include <d3d12.h>
#include <DirectXMath.h>

using namespace DirectX;

struct Particle
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT4 Color;
	DirectX::XMFLOAT3 StartVelocity;
	float Size;
	float Age;
};

struct ParticleVertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT2 UV;
	DirectX::XMFLOAT4 Color;
	float Size;
};

class Emitter
{
public:
	Emitter(
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

	void Update(float deltaTime);

	void UpdateSingleParticle(float deltaTime, int index);
	void SpawnParticle();

	void CopyParticlesToGPU();
	void CopyOneParticle(int index);
	void Draw();

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

	Particle* particles;
	int maxParticles;
	int firstDeadIndex;
	int firstAliveIndex;

	ParticleVertex* localParticleVertices;
};

