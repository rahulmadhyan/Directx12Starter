#include "Common.hlsl"

struct SKY_VS_INPUT
{
	float3 PosL	   : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC	   : TEXCOORD;
};

struct SKY_VS_OUTPUT
{
	float4 PosH	   : SV_POSITION;
	float3 PosL    : POSITION;
};

SKY_VS_OUTPUT main(SKY_VS_INPUT input)
{
	SKY_VS_OUTPUT output;

	// Use local vertex position as cubemap lookup vector
	output.PosL = input.PosL;

	// Transform to world space
	float3 posW = mul(float4(input.PosL, 1.0f), world);

	// Always centre sky about camera
	posW.xyz += eyePosW;

	// Have vieProjection in cb
	matrix viewProjection = mul(view, proj);

	// Set z = w so that z/w = 1 (i.e, skydome always on far plane).
	output.PosH = mul(posW, viewProjection).xyww;

	return output;
}