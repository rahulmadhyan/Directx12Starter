#include "Common.hlsl"

struct SKY_VS_OUTPUT
{
	float4 PosH	   : SV_POSITION;
	float3 PosL    : POSITION;
};

float4 main(SKY_VS_OUTPUT pixelInput) : SV_Target
{
	return cube.Sample(sampleLinear, pixelInput.PosL);
}