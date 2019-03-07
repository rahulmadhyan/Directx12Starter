#include "LightingUtil.hlsl"
#include "GPUParticleInclude.hlsl"
#include "SimplexNoise.hlsl"

cbuffer cbPerObject : register(b0)
{
	float4x4 world;
	float4x4 textureTransform;
}

cbuffer cbPass : register(b1)
{
	float4x4 view;
	float4x4 proj;
	float3 eyePosW;
	float cbPerObjectPad1;
	float4 ambientLight;
	float deltaTime;
	float totalTime;
	float aspectRatio;

	Light lights[MaxLights];
}

cbuffer cbMaterial : register(b2)
{
	float4 diffuseAlbedo;
	float3 fresnelR0;
	float  roughness;
	float4x4 materialTransform;
}

cbuffer particleData : register(b3)
{
	float4 startColor;
	float4 endColor;
	float3 velocity;
	float lifeTime;
	float3 acceleration;
	float pad;
	int emitCount;
	int maxParticles;
	int gridSize;
}

RWStructuredBuffer<Particle> ParticlePool		: register(u0);
AppendStructuredBuffer<uint> ADeadList			: register(u1);
RWStructuredBuffer<ParticleDraw> DrawList		: register(u2);
RWStructuredBuffer<uint> DrawArgs				: register(u3);

[numthreads(32, 1, 1)]
void main(uint id : SV_DispatchThreadID)
{
	if (id.x >= (uint)maxParticles)
		return;

	Particle particle = ParticlePool.Load(id.x);

	if (particle.Alive == 0.0f)
		return;

	particle.Age += deltaTime;
	particle.Alive = (float)(particle.Age < lifeTime);
	
	float agePercent = particle.Age / lifeTime;
	particle.Color = lerp(startColor, endColor, agePercent);

	particle.Position += particle.Velocity * deltaTime;
	
	// generate the particle's new velocity using 3D curl noise plugging its position into the noise function will giveus the velocity at that position - kind of like a flow field
	float3 curlPosition = particle.Position * 0.1f;
	float3 curlVelocity = curlNoise3D(curlPosition, 1.0f);
	particle.Velocity = curlVelocity * 2;

	//// put the particle back
	ParticlePool[id.x] = particle;

	// newly dead?
	if (particle.Alive == 0.0f)
	{
		// Add to dead list
		ADeadList.Append(id.x);
	}
	else
	{
		// increment the counter on the draw list, then put the new draw data at the returned (pre-increment) index
		uint drawIndex = DrawList.IncrementCounter();

		// set up draw data
		ParticleDraw drawData;
		drawData.Index = id.x; // this particle's actual index

		DrawList[drawIndex] = drawData;
	}
}
