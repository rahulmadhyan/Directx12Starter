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
	float3 Normal		: NORMAL;
	float2 UV			: TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	matrix viewProjection = mul(view, proj);
	output.position = mul(float4(input.position, 1.0f), viewProjection);

	//uv to offset position
	float2 offset = input.uv * 2 - 1;
	offset *= input.size;
	offset.y *= -1;
	output.position.xy += offset;

	output.uv = input.uv;
	output.color = input.color;

	return output;
}