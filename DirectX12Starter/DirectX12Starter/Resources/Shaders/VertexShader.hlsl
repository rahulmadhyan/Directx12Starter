#include "Common.hlsl"

struct VS_INPUT
{
	float3 Position		: POSITION;
	float3 Normal		: NORMAL;
	float2 UV			: TEXCOORD;
};

struct VS_OUTPUT
{
	float4 Position		: SV_POSITION;
	float3 Normal		: NORMAL;
	float2 UV			: TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	// Transform to world space

	float4 outPos = mul( float4(input.Position, 1.0f), world);
	matrix viewProjection = mul(view, proj);
	output.Position = mul(outPos, viewProjection);

	output.Normal = mul(input.Normal, (float3x3)world);

	float4 textureCoordinates = mul(float4(input.UV, 0.0, 1.0f), textureTransform);
	output.UV = mul(textureCoordinates, materialTransform).xy;

	return output;
}