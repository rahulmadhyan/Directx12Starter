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

[numthreads(1, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	// increment the counter to get the previous value, which happens to be how many particles we want to draw
	DrawArgs[0] = DrawList.IncrementCounter(); // vertexCountPerInstance (or index count if using an index buffer)
	DrawArgs[1] = 1; // instanceCount
	DrawArgs[2] = 0; // offsets
	DrawArgs[3] = 0; // offsets
	DrawArgs[4] = 0; // offsets
	DrawArgs[5] = 0; // offsets
	DrawArgs[6] = 0; // offsets
	DrawArgs[7] = 0; // offsets
	DrawArgs[8] = 0; // offsets
}