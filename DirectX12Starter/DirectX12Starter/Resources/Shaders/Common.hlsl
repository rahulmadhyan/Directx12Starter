#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 3
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 3
#endif


#include "LightingUtil.hlsl"

TextureCube cubeMap : register(t0);

Texture2D diffuseMap : register (t1);
Texture2D normalMap : register (t2);

SamplerState sampleLinear : register(s0);

cbuffer cbPerObject : register(b0)
{
	float4x4 world;
	float4x4 textureTransform;
};

cbuffer cbPass : register(b1)
{
	float4x4 view;
	float4x4 proj;
	float3 eyePosW;
	float cbPerObjectPad1;
	float4 ambientLight;

	Light lights[MaxLights];
};

cbuffer cbMaterial : register (b2)
{
	float4 diffuseAlbedo;
	float3 fresnelR0;
	float roughness;
	float4x4 materialTransform;
}

float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormal, float3 tangent)
{
	float3 normal = 2.0f * normalMapSample - 1.0f;

	float3 N = unitNormal;
	float3 T = normalize(tangent - dot(tangent, N) * N);
	float3 B = cross(N, T);

	float3x3 TBN = float3x3(T, B, N);

	float3 bumpedNormal = mul(normal, TBN);

	return bumpedNormal;
}