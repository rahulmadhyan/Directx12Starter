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

Texture2D    particleMap : register(t0);
SamplerState sampleLinear  : register(s0);

float4 main(VS_OUTPUT input) : SV_TARGET
{
	return particleMap.Sample(trilinear, input.uv) * input.color * input.color.a;
}
