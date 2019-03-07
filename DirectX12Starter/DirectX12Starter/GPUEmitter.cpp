#include "GPUEmitter.h"

GPUEmitter::GPUEmitter(int maxParticles,
	int gridSize,
	float emissionRate,
	float lifeTime,
	DirectX::XMFLOAT3 velocity,
	DirectX::XMFLOAT3 acceleration,
	DirectX::XMFLOAT4 startColor,
	DirectX::XMFLOAT4 endColor) :
	maxParticles(maxParticles),
	gridSize(gridSize),
	emissionRate(emissionRate),
	lifeTime(lifeTime),
	velocity(velocity),
	acceleration(acceleration),
	startColor(startColor),
	endColor(endColor)
{
	emitTimeCounter = 0.0f;
	timeBetweenEmit = 1.0f / emissionRate;
}

GPUEmitter::~GPUEmitter()
{

}

int GPUEmitter::GetEmitCount()
{
	return emitCount;
}

int GPUEmitter::GetMaxParticles()
{
	return maxParticles;
}

int GPUEmitter::GetGridSize()
{
	return gridSize;
}

int GPUEmitter::GetVerticesPerParticle()
{
	return 1;
}

float GPUEmitter::GetLifeTime()
{
	return lifeTime;
}

float GPUEmitter::GetEmitTimeCounter()
{
	return emitTimeCounter;
}

float GPUEmitter::GetTimeBetweenEmit()
{
	return timeBetweenEmit;
}

DirectX::XMFLOAT3 GPUEmitter::GetVelocity()
{
	return velocity;
}

DirectX::XMFLOAT3 GPUEmitter::GetAcceleration()
{
	return acceleration;
}

DirectX::XMFLOAT4 GPUEmitter::GetStartColor()
{
	return startColor;
}

DirectX::XMFLOAT4 GPUEmitter::GetEndColor()
{
	return endColor;
}

void GPUEmitter::SetEmitCount(int value)
{
	emitCount = value;
}

void GPUEmitter::SetEmitTimeCounter(float value)
{
	emitTimeCounter = value;
}

void GPUEmitter::Update(float TotalTime, float deltaTime)
{
	emitTimeCounter += deltaTime;
}