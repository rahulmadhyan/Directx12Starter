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

struct GS_OUTPUT
{
	float4 Position		: SV_POSITION;
	float4 Color		: COLOR;
	float2 UV			: TEXCOORD;
};

float4 main(GS_OUTPUT input) : SV_TARGET
{
	input.UV = input.UV * 2 - 1;

	float fade = saturate(distance(float2(0, 0), input.UV));
	float3 color = lerp(input.Color.rgb, float3(0, 0, 0), fade * fade);

	return float4(color, 1);
}