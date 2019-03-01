#include "LightingUtil.hlsl"

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

struct VS_INPUT
{
	float3 Position		: POSITION;
	float2 UV			: TEXCOORD;
	float4 Color		: COLOR;
	float Size : SIZE;
};

struct VS_OUTPUT
{
	float4 Position		: SV_POSITION;
	float2 UV			: TEXCOORD;
	float4 Color		: COLOR;
};

Texture2D    particleMap : register(t0);
SamplerState sampleLinear  : register(s0);

float4 main(VS_OUTPUT input) : SV_TARGET
{
	return particleMap.Sample(sampleLinear, input.UV) * input.Color * input.Color.a;
}
