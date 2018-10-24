
cbuffer cbPerObject : register(b0)
{
	float4x4 world;
};

cbuffer cbPass : register(b1)
{
	float4x4 view;
	float4x4 projection;
}

struct VS_INPUT
{
	float4 position : POSITION;
	float4 color	: COLOR;
};

struct VS_OUTPUT
{
	float4 position : SV_POSITION;
	float4 color	: COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	output.position = mul(input.position, world);
	output.position = mul(output.position, view);
	output.position = mul(output.position, projection);

	output.color = input.color;
	return output;
}