
Texture2D    diffuseMap : register(t0);
SamplerState sampleLinear  : register(s0);

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
	float4 color = diffuseMap.Sample(sampleLinear, input.UV);

	return color;
}