#include "LightingUtil.hlsl"

cbuffer cbPerObject : register(b0)
{
	float4x4 world;
	float4x4 textureTransform;
}

cbuffer cbPass : register(b1)
{
	float4x4 view;
	float4x4 proj;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;

	float3 eyePosW;
	float cbPerObjectPad1;

	float2 gRenderTargetSize;
	float2 gInvRenderTargetSize;

	float aspectRatio;
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
}

cbuffer particleData : register(b3)
{
	float4 startColor;
	float4 endColor;
	float3 velocity;
	float lifeTime;
	float3 acceleration;
	float pad;
	int emitCount;
	int maxParticles;
	int gridSize;
}

struct VS_INPUT
{
	float3 Position		: POSITION;
	float3 Normal		: NORMAL;
	float2 UV			: TEXCOORD;
};

struct VS_OUTPUT
{
	float4 Position		: SV_POSITION;
	float3 PositionW	: POSITION;
	float3 Normal		: NORMAL;
	float2 UV			: TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	// Transform to world space

	float4 outPos = mul( float4(input.Position, 1.0f), world);

	output.PositionW = outPos.xyz;

	matrix viewProjection = mul(view, proj);
	output.Position = mul(outPos, viewProjection);

	output.Normal = mul(input.Normal, (float3x3)world);

	float4 textureCoordinates = mul(float4(input.UV, 0.0, 1.0f), textureTransform);
	output.UV = mul(textureCoordinates, materialTransform).xy;

	return output;
}