#pragma once
#include "d3dUtil.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "Vertex.h"

struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TextureTransform = MathHelper::Identity4x4();
};

struct PassConstants
{
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 CameraPosition = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	float pad1 = 0.0f;
	DirectX::XMFLOAT4 ambientLight = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	float DeltaTime;
	float TotalTime;
	float AspectRatio;
	float pad2 = 0.0f;
	Light lights[MAX_LIGHTS];
};

struct GPUParticleConstants
{
	DirectX::XMFLOAT4 startColor;
	DirectX::XMFLOAT4 endColor;
	DirectX::XMFLOAT3 velocity;
	float LifeTime;
	DirectX::XMFLOAT3 acceleration;
	float pad = 0.0f;
	int EmitCount = 0;
	int MaxParticles = 0;
	int GridSize = 0;
};

struct ParticleVertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT2 UV;
	DirectX::XMFLOAT4 Color;
	float Size;
};

// stores the resources needed for the CPU to build the command lists for a frame 
struct FrameResource
{
public:

	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount, UINT particleCount, UINT GPUParticleCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	// we cannot reset the allocator until the GPU is done processing the commands so each frame needs their own allocator
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandListAllocator;

	// we cannot update a cbuffer until the GPU is done processing the commands that reference it
	//so each frame needs their own cbuffers
	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialConstants>> MaterialCB = nullptr;
	std::unique_ptr<UploadBuffer<GPUParticleConstants>> GPUParticleCB = nullptr;

	//emitter dynamic vertex buffer
	std::unique_ptr<UploadBuffer<ParticleVertex>> emitterVB = nullptr;

	// fence value to mark commands up to this fence point 
	// this lets us check if these frame resources are still in use by the GPU.
	UINT64 Fence = 0;
};