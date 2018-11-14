#include "LightingUtil.h"

Texture2D    diffuseMap : register(t0);
SamplerState sampleLinear  : register(s0);

cbuffer cbPass : register(b1)
{
	float4x4 view;
	float4x4 invView;
	float4x4 proj;
	float4x4 pnvProj;
	float4x4 viewProj;
	float4x4 invViewProj;
	float3 eyePosW;
	float cbPerObjectPad1;
	float2 renderTargetSize;
	float2 invRenderTargetSize;
	float nearZ;
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
};

struct VS_OUTPUT
{
	float4 Position		: SV_POSITION;
	float3 Normal		: NORMAL;
	float2 UV			: TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
	input.Normal = normalize(input.Normal);

	float toEyeNormal = normalize(eyePosW - input.Position);

	// ambient light
	float4 ambient = ambientLight * diffuseAlbedo;

	const float shininess = 1.0f - roughness;
	Material mat = { diffuseAlbedo, fresnelR0, shininess };
	float3 shadowFactor = 1.0f;
	float4 directLight = ComputeLighting(lights, mat, input.Position, input.Normal, toEyeNormal, shadowFactor);

	float4 litColor = ambient + directLight;

	litColor.a = diffuseAlbedo.a;

	return litColor;
}