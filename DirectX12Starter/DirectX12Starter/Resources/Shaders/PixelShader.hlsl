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

Texture2D    diffuseMap : register(t0);
SamplerState sampleLinear  : register(s0);

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
};

struct VS_OUTPUT
{
	float4 Position		: SV_POSITION;
	float3 Normal		: NORMAL;
	float2 UV			: TEXCOORD;
};

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