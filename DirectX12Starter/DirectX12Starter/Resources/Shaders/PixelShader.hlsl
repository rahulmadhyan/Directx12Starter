// Defualts for number of lights.
#ifndef NUM_DIR_LIGHTS
	#define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
	#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
	#define NUM_SPOT_LIGHTS 0
#endif

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

Texture2D    diffuseMap : register(t0);
SamplerState sampleLinear  : register(s0);

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float4 difAlbedo = diffuseMap.Sample(sampleLinear, input.UV) * diffuseAlbedo;
	input.Normal = normalize(input.Normal);

	float3 toEyeNormal = normalize(eyePosW - input.Position.xyz);

	// ambient light
	float4 ambient = ambientLight * difAlbedo;

	const float shininess = 1.0f - roughness;
	Material mat = { difAlbedo, fresnelR0, shininess };
	float3 shadowFactor = 1.0f;
	float4 directLight = ComputeLighting(lights, mat, input.Position.xyz, input.Normal, toEyeNormal, shadowFactor);

	float4 litColor = directLight + ambient;

	litColor.a = difAlbedo.a;

	return litColor;
}