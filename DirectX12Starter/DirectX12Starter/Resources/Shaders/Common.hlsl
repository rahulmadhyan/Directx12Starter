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