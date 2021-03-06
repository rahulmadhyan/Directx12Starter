#include "LightingUtil.hlsl"

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
}

cbuffer cbMaterial : register(b2)
{
	float4 diffuseAlbedo;
	float3 fresnelR0;
	float  roughness;
	float4x4 materialTransform;
};

struct VS_INPUT
{
	float3 Position		: POSITION;
	float3 Normal		: NORMAL;
	float2 UV			: TEXCOORD;
};

struct VS_OUTPUT
{
	float4 Position		: SV_POSITION;
	float3 PositionL	: POSITION;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	output.PositionL = input.Position;

	// tranform to world space
	float4 positionWorld = mul(float4(input.Position, 1.0f), world);

	// center about camera
	positionWorld.xyz += eyePosW;

	matrix viewProjection = mul(view, proj);

	// Set z = w so that z/w = 1 (i.e., skydome always on far plane).
	output.Position = mul(positionWorld, viewProjection).xyww;

	return output;
}