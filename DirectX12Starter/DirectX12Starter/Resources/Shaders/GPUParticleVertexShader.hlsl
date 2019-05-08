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
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;

	float3 eyePosW;
	float cbPerObjectPad1;

	float2 gRenderTargetSize;
	float2 gInvRenderTargetSize;

	float aspectRatio;
	float farZ;
	float totalTime;
	float deltaTime;

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

struct VS_OUTPUT
{
	float3 Position		: POSITION;
	float Size			: SIZE;
	float4 Color		: COLOR;
};

StructuredBuffer<Particle> ParticlePool		: register(t1);
StructuredBuffer<ParticleDraw> DrawList		: register(t2);

VS_OUTPUT main(uint id : SV_VertexID)
{
	VS_OUTPUT output;

	ParticleDraw draw = DrawList.Load(id);
	Particle particle = ParticlePool.Load(draw.Index);

	// pass through
	output.Position = particle.Position;
	output.Size = particle.Size;
	output.Color = particle.Color;

	return output;
}