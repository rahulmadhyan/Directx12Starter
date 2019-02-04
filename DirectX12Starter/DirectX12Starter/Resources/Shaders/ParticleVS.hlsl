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
	float2 UV			: TEXCOORD;
	float4 Color		: COLOR;
	float Size			: SIZE;
};						 

struct VS_OUTPUT
{
	float4 Position		: SV_POSITION;
	float2 UV			: TEXCOORD;
	float4 Color		: COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	matrix viewProjection = mul(view, proj);
	output.Position = mul(float4(input.Position, 1.0f), viewProjection);

	//uv to offset position
	float2 offset = input.UV * 2 - 1;
	offset *= input.Size;
	offset.y *= -1;
	output.Position.xy += offset;

	output.UV = input.UV;
	output.Color = input.Color;

	return output;
}