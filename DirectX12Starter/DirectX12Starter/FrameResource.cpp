#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount, UINT particleCount, UINT GPUParticleCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(commandListAllocator.GetAddressOf())));

	PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
	MaterialCB = std::make_unique<UploadBuffer<MaterialConstants>>(device, materialCount, true);
	GPUParticleCB = std::make_unique<UploadBuffer<GPUParticleConstants>>(device, GPUParticleCount, true);

	emitterVB = std::make_unique<UploadBuffer<ParticleVertex>>(device, particleCount, false);
}

FrameResource::~FrameResource()
{

}