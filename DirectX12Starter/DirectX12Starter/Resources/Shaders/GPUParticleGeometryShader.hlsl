#include "LightingUtil.hlsl"
#include "GPUParticleInclude.hlsl"
#include "SimplexNoise.hlsl"

cbuffer cbPerObject : register(b0)
{
	float4x4 world;
	float4x4 textureTransform;
}

cbuffer cbPass : register(b1)
{
	float4x4 view;
	float4x4 proj;
	float3 eyePosW;
	float cbPerObjectPad1;
	float4 ambientLight;
	float deltaTime;
	float totalTime;
	float aspectRatio;

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

struct VS_OUTPUT
{
	float3 Position		: POSITION;
	float Size			: SIZE;
	float4 Color		: COLOR;
};

struct GS_OUTPUT
{
	float4 Position		: SV_POSITION;
	float4 Color		: COLOR;
	float2 UV			: TEXCOORD;
};

[maxvertexcount(4)]
void main(point VS_OUTPUT input[1], inout TriangleStream<GS_OUTPUT> outStream)
{
	GS_OUTPUT output;

	float2 offsets[4];
	offsets[0] = float2(-1.0f, -1.0f);
	offsets[1] = float2(-1.0f, +1.0f);
	offsets[2] = float2(+1.0f, -1.0f);
	offsets[3] = float2(+1.0f, +1.0f);

	matrix mvp = mul(mul(world, view), proj);

	[unroll]
	for (int i = 0; i < 4; i++)
	{
		output.Position = mul(float4(input[0].Position, 1.0f), mvp);

		float depthChange = output.Position.z / output.Position.w;

		// Adjust based on depth (prevents particles from getting too large)
		offsets[i].y *= aspectRatio;
		output.Position.xy += offsets[i] * depthChange * input[0].Size;
		output.Color = input[0].Color;
		output.UV = saturate(offsets[i]);

		// Done
		outStream.Append(output);
	}
}