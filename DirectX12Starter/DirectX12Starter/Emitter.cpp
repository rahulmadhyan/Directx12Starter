#include "Emitter.h"

Emitter::Emitter(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList,
	int maxParticles,
	int particlesPerSecond,
	float lifetime,
	float startSize,
	float endSize,
	DirectX::XMFLOAT4 startColor,
	DirectX::XMFLOAT4 endColor,
	DirectX::XMFLOAT3 startVelocity,
	DirectX::XMFLOAT3 emitterPosition,
	DirectX::XMFLOAT3 emitterAcceleration)
{
	this->device = device,
	this->commandList = commandList,
	this->maxParticles = maxParticles;
	this->lifetime = lifetime;
	this->startColor = startColor;
	this->endColor = endColor;
	this->startVelocity = startVelocity;
	this->startSize = startSize;
	this->endSize = endSize;
	this->particlesPerSecond = particlesPerSecond;
	this->secondsPerParticle = 1.0f / particlesPerSecond;

	this->emitterPosition = emitterPosition;
	this->emitterAcceleration = emitterAcceleration;
	
	timeSinceEmit = 0;
	livingParticleCount = 0;
	firstAliveIndex = 0;
	firstDeadIndex = 0;

	particles = new Particle[maxParticles];
	localParticleVertices = new ParticleVertex[4 * maxParticles];
	for (int i = 0; i < maxParticles * 4; i += 4)
	{
		localParticleVertices[i + 0].UV = DirectX::XMFLOAT2(0, 0);
		localParticleVertices[i + 1].UV = DirectX::XMFLOAT2(1, 0);
		localParticleVertices[i + 2].UV = DirectX::XMFLOAT2(1, 1);
		localParticleVertices[i + 3].UV = DirectX::XMFLOAT2(0, 1);
	}

	indices = new uint16_t[maxParticles * 6];
	int indexCount = 0;
	for (int i = 0; i < maxParticles * 4; i += 4)
	{
		indices[indexCount++] = i;
		indices[indexCount++] = i + 1;
		indices[indexCount++] = i + 2;
		indices[indexCount++] = i;
		indices[indexCount++] = i + 2;
		indices[indexCount++] = i + 3;
	}
}

Emitter::~Emitter()
{
	delete[] particles;
	delete[] localParticleVertices;
	delete[] indices;
}

int Emitter::GetMaxParticles()
{
	return maxParticles;
}

ParticleVertex* Emitter::GetParticleVertices()
{
	return localParticleVertices;
}

uint16_t* Emitter::GetParticleIndices()
{
	return indices;
}

DirectX::XMFLOAT3 Emitter::GetEmitterPosition()
{
	return emitterPosition;
}

void Emitter::SetEmitterPosition(float x, float y, float z)
{
	emitterPosition = DirectX::XMFLOAT3(x, y, z);
}

void Emitter::SpawnParticles()
{
	for (size_t i = 0; i < maxParticles; i++)
	{
		SpawnParticle();
	}
}

void Emitter::Update(float deltaTime)
{
	if (firstAliveIndex < firstDeadIndex)
	{
		for (int i = firstAliveIndex; i < firstDeadIndex; i++)
			UpdateSingleParticle(deltaTime, i);
	}
	else
	{
		for (int i = firstAliveIndex; i < maxParticles; i++)
			UpdateSingleParticle(deltaTime, i);

		for (int i = 0; i < firstDeadIndex; i++)
			UpdateSingleParticle(deltaTime, i);
	}

	timeSinceEmit += deltaTime;

	/*while (timeSinceEmit > secondsPerParticle)
	{
		SpawnParticle();
		timeSinceEmit -= secondsPerParticle;
	}*/

	CopyParticlesToGPU();
}

void Emitter::UpdateSingleParticle(float deltaTime, int index)
{
	if (particles[index].Age >= lifetime)
		return;

	particles[index].Age += deltaTime;
	if (particles[index].Age >= lifetime)
	{
		firstAliveIndex++;
		firstAliveIndex %= maxParticles;
		livingParticleCount--;
		return;
	}

	float agePercent = particles[index].Age / lifetime;

	DirectX::XMStoreFloat4(
		&particles[index].Color,
		DirectX::XMVectorLerp(
			DirectX::XMLoadFloat4(&startColor),
			DirectX::XMLoadFloat4(&endColor),
			agePercent));

	particles[index].Size = startSize + agePercent * (endSize - startSize);

	DirectX::XMVECTOR startPosition = DirectX::XMLoadFloat3(&emitterPosition);
	DirectX::XMVECTOR startVelocity = DirectX::XMLoadFloat3(&particles[index].StartVelocity);
	DirectX::XMVECTOR acceleration = DirectX::XMLoadFloat3(&emitterAcceleration);
	float t = particles[index].Age;

	DirectX::XMStoreFloat3(
		&particles[index].Position,
		acceleration * t * t / 2.0f + startVelocity * t + startPosition);
}

void Emitter::SpawnParticle()
{
	if (livingParticleCount == maxParticles)
		return;

	particles[firstDeadIndex].Age = 0;
	particles[firstDeadIndex].Size = startSize;
	particles[firstDeadIndex].Color = startColor;
	particles[firstDeadIndex].Position = emitterPosition;
	particles[firstDeadIndex].StartVelocity = startVelocity;
	particles[firstDeadIndex].StartVelocity.x += ((float)rand() / RAND_MAX) * 0.4f - 0.2f;
	particles[firstDeadIndex].StartVelocity.y += ((float)rand() / RAND_MAX) * 0.4f - 0.2f;
	particles[firstDeadIndex].StartVelocity.z += ((float)rand() / RAND_MAX) * 0.4f - 0.2f;

	firstDeadIndex++;
	firstDeadIndex %= maxParticles;

	livingParticleCount++;
}

void Emitter::CopyParticlesToGPU()
{
	// Check cyclic buffer status
	if (firstAliveIndex < firstDeadIndex)
	{
		for (int i = firstAliveIndex; i < firstDeadIndex; i++)
			CopyOneParticle(i);
	}
	else
	{
		// Update first half (from firstAlive to max particles)
		for (int i = firstAliveIndex; i < maxParticles; i++)
			CopyOneParticle(i);

		// Update second half (from 0 to first dead)
		for (int i = 0; i < firstDeadIndex; i++)
			CopyOneParticle(i);
	}
}

void Emitter::CopyOneParticle(int index)
{
	int i = index * 4;

	localParticleVertices[i + 0].Position = particles[index].Position;
	localParticleVertices[i + 1].Position = particles[index].Position;
	localParticleVertices[i + 2].Position = particles[index].Position;
	localParticleVertices[i + 3].Position = particles[index].Position;

	localParticleVertices[i + 0].Size = particles[index].Size;
	localParticleVertices[i + 1].Size = particles[index].Size;
	localParticleVertices[i + 2].Size = particles[index].Size;
	localParticleVertices[i + 3].Size = particles[index].Size;

	localParticleVertices[i + 0].Color = particles[index].Color;
	localParticleVertices[i + 1].Color = particles[index].Color;
	localParticleVertices[i + 2].Color = particles[index].Color;
	localParticleVertices[i + 3].Color = particles[index].Color;
}


