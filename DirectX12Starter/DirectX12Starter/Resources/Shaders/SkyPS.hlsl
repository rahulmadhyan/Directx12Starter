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

TextureCube cubeMap			: register(t0);
SamplerState sampleLinear	: register(s0);

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
	float3 PositionL	: POSITION;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	return cubeMap.Sample(sampleLinear, input.PositionL);
}