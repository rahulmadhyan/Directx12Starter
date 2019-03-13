#include "Game.h"

const int gNumberFrameResources = 3;

#ifdef _DEBUG
void Game::InitDebugDraw()
{
	graphicsMemory = std::make_unique<GraphicsMemory>(Device.Get());

	batch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(Device.Get());

	RenderTargetState rtState(BackBufferFormat, DepthStencilFormat);

	EffectPipelineStateDescription pd(
		&VertexPositionColor::InputLayout,
		CommonStates::Opaque,
		CommonStates::DepthDefault,
		CommonStates::CullNone,
		rtState,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);

	effect = std::make_unique<BasicEffect>(Device.Get(), EffectFlags::VertexColor, pd);
}

void Game::DebugDraw(ID3D12GraphicsCommandList* cmdList, const std::vector<Entity*> entities)
{
	// For each render item...
	for (size_t i = 0; i < entities.size(); ++i)
	{
		auto e = entities[i];

		const XMFLOAT4X4* currentWorldMatrix = systemData->GetWorldMatrix(e->SystemWorldIndex);
		XMMATRIX world = DirectX::XMLoadFloat4x4(currentWorldMatrix);

		XMMATRIX proj = DirectX::XMLoadFloat4x4(&mainCamera.GetProjectionMatrix());

		XMMATRIX view = DirectX::XMLoadFloat4x4(&mainCamera.GetViewMatrix());

		effect->SetMatrices(world, view, proj);

		effect->Apply(cmdList);
		batch->Begin(cmdList);

		DX::Draw(batch.get(), e->meshData.Bounds, DirectX::Colors::Red);

		batch->End();
	}
}

void Game::DebugDraw(ID3D12GraphicsCommandList* cmdList, const std::vector<EnemyEntity*> entities)
{
	// For each render item...
	for (size_t i = 0; i < entities.size(); ++i)
	{
		auto e = entities[i];

		const XMFLOAT4X4* currentWorldMatrix = systemData->GetWorldMatrix(e->SystemWorldIndex);
		XMMATRIX world = DirectX::XMLoadFloat4x4(currentWorldMatrix);

		XMMATRIX proj = DirectX::XMLoadFloat4x4(&mainCamera.GetProjectionMatrix());

		XMMATRIX view = DirectX::XMLoadFloat4x4(&mainCamera.GetViewMatrix());

		effect->SetMatrices(world, view, proj);

		effect->Apply(cmdList);
		batch->Begin(cmdList);

		DX::Draw(batch.get(), e->meshData.Bounds, DirectX::Colors::Red);

		batch->End();
	}
}

void Game::DebugDrawPlayerRay(ID3D12GraphicsCommandList* cmdList, const std::vector<Entity*> entities)
{
	const Ray* playerRay = player->GetRay();

	auto e = entities[0];

	const XMFLOAT4X4* currentWorldMatrix = systemData->GetWorldMatrix(e->SystemWorldIndex);
	XMMATRIX world = DirectX::XMLoadFloat4x4(currentWorldMatrix);

	XMMATRIX proj = DirectX::XMLoadFloat4x4(&mainCamera.GetProjectionMatrix());

	XMMATRIX view = DirectX::XMLoadFloat4x4(&mainCamera.GetViewMatrix());

	effect->Apply(cmdList);
	batch->Begin(cmdList);

	XMVECTOR origin = XMVectorSet(0.0f, 0.0f, 0.0, 0.0f);

	XMVECTOR direction = XMLoadFloat3(&playerRay->direction);
	direction = XMVectorScale(direction, playerRay->distance);

	DX::DrawRay(batch.get(), origin, direction, false, DirectX::Colors::Red);

	batch->End();
}
#endif // _DEBUG

Game::Game(HINSTANCE hInstance) : DXCore(hInstance)
{

}

Game::~Game()
{
	if (Device != nullptr)
		FlushCommandQueue();

	delete systemData;
	systemData = 0;

	delete inputManager;
	
	delete player;

	delete enemies;

	delete waypoints;

	delete gpuEmitter;
}

bool Game::Initialize()
{
	if (!DXCore::Initialize())
		return false;

	// reset the command list to prep for initialization commands
	ThrowIfFailed(CommandList->Reset(CommandListAllocator.Get(), nullptr));

#ifdef _DEBUG
	InitDebugDraw();
#endif // _DEBUG

	systemData = new SystemData();

	mainCamera = Camera(screenWidth, screenHeight);

	inputManager = InputManager::getInstance();

	player = new Player(Device.Get(), CommandList.Get(), systemData);

	enemies = new Enemies(systemData);

	waypoints = new Waypoints(systemData);

	gpuEmitter = new GPUEmitter(
		100000,
		1000,
		100000.0f,
		1000.0f,
		XMFLOAT3(1.0f, 1.0f, -5.0f),
		XMFLOAT3(0.0f, 0.0f, 0.0f),
		XMFLOAT4(0.85f, 0.85f, 0.85f, 1.0f),
		XMFLOAT4(0.5f, 0.5f, 0.5f, 0.5f)
	);

	BuildTextures();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildGeometry();
	BuildMaterials();
	BuildEntities();
	BuildFrameResources();
	BuildDescriptorHeaps();
	BuildConstantBufferViews();
	BuildPSOs();

	enemies->SetPlayerEntity(playerEntities[0]);
	enemies->SetEnemyEntitites(enemyEntities);
	enemies->SetWaypointEntitites(waypointEntities);

	// execute the initialization commands
	ThrowIfFailed(CommandList->Close());
	ID3D12CommandList* cmdsLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// wait until initialization is complete
	FlushCommandQueue();
	
	// gpu particle init
	ThrowIfFailed(CommandListAllocator->Reset());

	ThrowIfFailed(CommandList->Reset(CommandListAllocator.Get(), PSOs["particleDeadList"].Get()));

	CommandList->SetComputeRootSignature(rootSignature.Get());

	currentFrameResourceIndex = (currentFrameResourceIndex + 1) % gNumberFrameResources;
	currentFrameResource = FrameResources[currentFrameResourceIndex].get();

	UpdateMainPassCB(timer);
	UpadteGPUParticleCBs(timer);

	ID3D12DescriptorHeap* objDescriptorHeaps[] = { CBVHeap.Get() };
	CommandList->SetDescriptorHeaps(_countof(objDescriptorHeaps), objDescriptorHeaps);

	int passCbvIndex = PassCbvOffset + currentFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, CBVSRVUAVDescriptorSize);
	CommandList->SetComputeRootDescriptorTable(1, passCbvHandle);

	int gpuParticleCBVIndex = GPUParticleCBVOffset + currentFrameResourceIndex;
	auto gpuParticleCBVHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVHeap->GetGPUDescriptorHandleForHeapStart());
	gpuParticleCBVHandle.Offset(gpuParticleCBVIndex, CBVSRVUAVDescriptorSize);
	CommandList->SetComputeRootDescriptorTable(4, gpuParticleCBVHandle);

	ID3D12DescriptorHeap* descriptorHeaps[] = { GPUParticleSRVUAVHeap.Get() };
	CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	CommandList->SetComputeRootDescriptorTable(7, GPUParticleResources["ParticlePool"]->GPUAVHandle);
	CommandList->SetComputeRootDescriptorTable(8, GPUParticleResources["DeadList"]->GPUAVHandle);
	CommandList->SetComputeRootDescriptorTable(9, GPUParticleResources["DrawList"]->GPUAVHandle);
	CommandList->SetComputeRootDescriptorTable(10, GPUParticleResources["DrawArgs"]->GPUAVHandle);

	CommandList->Dispatch(gpuEmitter->GetMaxParticles(), 1, 1);

	ThrowIfFailed(CommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists1[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists1);

	// Wait for the work to finish.
	FlushCommandQueue();

	return true;
}

void Game::Resize()
{
	DXCore::Resize();

	mainCamera.SetProjectionMatrix(screenWidth, screenHeight);
}

void Game::Update(const Timer &timer)
{
	// Cycle through the circular frame resource array.
	currentFrameResourceIndex = (currentFrameResourceIndex + 1) % gNumberFrameResources;
	currentFrameResource = FrameResources[currentFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (currentFrameResource->Fence != 0 && Fence->GetCompletedValue() < currentFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(Fence->SetEventOnCompletion(currentFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	mainCamera.Update();
	inputManager->UpdateController();

	player->Update(timer, playerEntities[0], enemyEntities);
	enemies->Update(timer);

	//update emitter vertex buffer
	auto currentEmitterVB = currentFrameResource->emitterVB.get();
	ParticleVertex* currentVertices = player->GetEmitter()->GetParticleVertices();
	for (int i = 0; i < player->GetEmitter()->GetMaxParticles() * 4; ++i)
	{
		ParticleVertex p = currentVertices[i];
		currentEmitterVB->CopyData(i, p);
	}

	emitterEntities[0]->Geo->VertexBufferGPU = currentEmitterVB->Resource();

	// gpu particle update
	gpuEmitter->Update(timer.GetTotalTime(), timer.GetDeltaTime());

	UpdateObjectCBs(timer);
	UpdateMainPassCB(timer);
	UpadteMaterialCBs(timer);
	UpadteGPUParticleCBs(timer);
}

void Game::Draw(const Timer &timer)
{
	auto currentCommandListAllocator = currentFrameResource->commandListAllocator;

	// reuse the memory associated with command recording
	// we can only reset when the associated command lists have finished execution on the GPU
	ThrowIfFailed(currentCommandListAllocator->Reset());

	ThrowIfFailed(CommandList->Reset(currentCommandListAllocator.Get(), PSOs["opaque"].Get()));
	
	CommandList->RSSetViewports(1, &ScreenViewPort);
	CommandList->RSSetScissorRects(1, &ScissorRect);

	// indicate a state transition on the resource usage
	CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// clear the back buffer and depth buffer
	CommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::Black, 0, nullptr);
	CommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// specify the buffers we are going to render to
	CommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* objDescriptorHeaps[] = { CBVHeap.Get() };
	CommandList->SetDescriptorHeaps(_countof(objDescriptorHeaps), objDescriptorHeaps);

	CommandList->SetGraphicsRootSignature(rootSignature.Get());

	int passCbvIndex = PassCbvOffset + currentFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, CBVSRVUAVDescriptorSize);
	CommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

	DrawEntities(CommandList.Get(), playerEntities);
	DrawEntities(CommandList.Get(), sceneEntities);
	DrawEntities(CommandList.Get(), enemyEntities);
	//DrawEntities(CommandList.Get(), waypointEntities);
	
	CommandList->SetPipelineState(PSOs["sky"].Get());
	DrawEntities(CommandList.Get(), skyEntities);

	CommandList->SetPipelineState(PSOs["emitter"].Get());
	DrawEntities(CommandList.Get(), emitterEntities);

	DrawGPUParticles(CommandList.Get());

#ifdef _DEBUG
	DebugDraw(CommandList.Get(), playerEntities);
	DebugDrawPlayerRay(CommandList.Get(), playerEntities);
	DebugDraw(CommandList.Get(), enemyEntities);
	graphicsMemory->Commit(CommandQueue.Get());
#endif // _DEBUG

	// indicate a state transition on the resource usage
	CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// done recording commands
	ThrowIfFailed(CommandList->Close());

	// add the command list to the queue for execution
	ID3D12CommandList* cmdsLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// wwap the back and front buffers
	ThrowIfFailed(SwapChain->Present(0, 0));
	currentBackBuffer = (currentBackBuffer + 1) % SwapChainBufferCount;

	// advance the fence value to mark commands up to this fence point
	currentFrameResource->Fence = ++currentFence;

	// add an instruction to the command queue to set a new fence point.
	// because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal()
	CommandQueue->Signal(Fence.Get(), currentFence);
}

void Game::UpdateObjectCBs(const Timer & timer)
{
	auto currentObjectCB = currentFrameResource->ObjectCB.get();
	for (auto& e : allEntities)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			const XMFLOAT4X4* currentWorldMatrix = systemData->GetWorldMatrix(e->SystemWorldIndex);
 			XMMATRIX world = XMLoadFloat4x4(currentWorldMatrix);
			XMMATRIX textureTransform = XMLoadFloat4x4(&e->TextureTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TextureTransform, XMMatrixTranspose(textureTransform));

			currentObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void Game::UpdateMainPassCB(const Timer &timer)
{
	XMMATRIX view = XMLoadFloat4x4(&mainCamera.GetViewMatrix());
	XMMATRIX projection = XMLoadFloat4x4(&mainCamera.GetProjectionMatrix());

	XMMATRIX viewProj = XMMatrixMultiply(view, projection);

	XMStoreFloat4x4(&MainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&MainPassCB.Proj, XMMatrixTranspose(projection));

	MainPassCB.CameraPosition = mainCamera.GetCameraPosition();
	MainPassCB.ambientLight = XMFLOAT4(0.2f, 0.2f, 0.2f, 0.5f);

	MainPassCB.DeltaTime = timer.GetDeltaTime();
	MainPassCB.TotalTime = timer.GetTotalTime();

	MainPassCB.AspectRatio = (float)screenWidth / screenHeight;

	MainPassCB.lights[0].Direction = { 1.0f, 0.0f, 0.0f };
	MainPassCB.lights[0].Strength = { 0.7f, 0.7f, 0.7f };

	auto currPassCB = currentFrameResource->PassCB.get();
	currPassCB->CopyData(0, MainPassCB);
}

void Game::UpadteMaterialCBs(const Timer& timet)
{
	auto currentMaterialCB = currentFrameResource->MaterialCB.get();
	for (auto& e : Materials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX materialTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants materialConstants;
			materialConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			materialConstants.FresnelR0 = mat->FresnelR0;
			materialConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&materialConstants.MatTransform, XMMatrixTranspose(materialTransform));

			currentMaterialCB->CopyData(mat->MatCBIndex, materialConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void Game::UpadteGPUParticleCBs(const Timer& timet)
{
	MainGPUParticleCB.startColor = gpuEmitter->GetStartColor();
	MainGPUParticleCB.endColor = gpuEmitter->GetEndColor();
	MainGPUParticleCB.velocity = gpuEmitter->GetVelocity();
	MainGPUParticleCB.LifeTime = gpuEmitter->GetLifeTime();
	MainGPUParticleCB.acceleration = gpuEmitter->GetAcceleration();
	MainGPUParticleCB.EmitCount = gpuEmitter->GetEmitCount();
	MainGPUParticleCB.MaxParticles = gpuEmitter->GetMaxParticles();
	MainGPUParticleCB.GridSize = gpuEmitter->GetGridSize();

	auto currentParticleCB = currentFrameResource->GPUParticleCB.get();
	currentParticleCB->CopyData(0, MainGPUParticleCB);
}

void Game::BuildTextures()
{
	auto demo1Texture = std::make_unique<Texture>();
	demo1Texture->Name = "1";
	demo1Texture->Filename = L"Resources/Textures/Demo1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(Device.Get(),
		CommandList.Get(), demo1Texture->Filename.c_str(),
		demo1Texture->Resource,
		demo1Texture->UploadHeap));

	Textures[demo1Texture->Name] = std::move(demo1Texture);

	auto demo2Texture = std::make_unique<Texture>();
	demo2Texture->Name = "2";
	demo2Texture->Filename = L"Resources/Textures/Demo2.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(Device.Get(),
		CommandList.Get(), demo2Texture->Filename.c_str(),
		demo2Texture->Resource,
		demo2Texture->UploadHeap));

	Textures[demo2Texture->Name] = std::move(demo2Texture);

	auto emitterTexture = std::make_unique<Texture>();
	emitterTexture->Name = "3";
	emitterTexture->Filename = L"Resources/Textures/FireParticle.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(Device.Get(),
		CommandList.Get(), emitterTexture->Filename.c_str(),
		emitterTexture->Resource,
		emitterTexture->UploadHeap));

	Textures[emitterTexture->Name] = std::move(emitterTexture);

	auto cubeMapTexture = std::make_unique<Texture>();
	cubeMapTexture->Name = "4";
	cubeMapTexture->Filename = L"Resources/Textures/PurpleCube.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(Device.Get(),
		CommandList.Get(), cubeMapTexture->Filename.c_str(),
		cubeMapTexture->Resource,
		cubeMapTexture->UploadHeap));

	CubeMapTextures[cubeMapTexture->Name] = std::move(cubeMapTexture);

	// gpu particle resources
	auto particlePoolResource = std::make_unique<GPUParticleTexture>();
	particlePoolResource->Name = "ParticlePool";
	particlePoolResource->NumberOfElements = gpuEmitter->GetMaxParticles();
	particlePoolResource->StructByStride = sizeof(GPUParticle);
	GPUParticleResources[particlePoolResource->Name] = std::move(particlePoolResource);

	auto deadListResource = std::make_unique<GPUParticleTexture>();
	deadListResource->Name = "DeadList";
	deadListResource->NumberOfElements = gpuEmitter->GetMaxParticles();
	deadListResource->StructByStride = sizeof(unsigned int);
	GPUParticleResources[deadListResource->Name] = std::move(deadListResource);

	auto drawListResource = std::make_unique<GPUParticleTexture>();
	drawListResource->Name = "DrawList";
	drawListResource->NumberOfElements = gpuEmitter->GetMaxParticles();
	drawListResource->StructByStride = sizeof(GPUParticleSort);
	GPUParticleResources[drawListResource->Name] = std::move(drawListResource);

	auto drawArgsResource = std::make_unique<GPUParticleTexture>();
	drawArgsResource->Name = "DrawArgs";
	drawArgsResource->NumberOfElements = 9;
	drawArgsResource->StructByStride = sizeof(unsigned int);
	GPUParticleResources[drawArgsResource->Name] = std::move(drawArgsResource);
}

void Game::BuildDescriptorHeaps()
{
	// build constant buffer heap for objects
	UINT objCount = (UINT)allEntities.size();

	// Need a CBV descriptor for each object for each frame resource,
	// +1 for the perPass CBV for each frame resource
	// +1 for GPUParticle CBV for each frame resource
	UINT objNumberDescriptors = (objCount + 2) * gNumberFrameResources;

	// Save an offset to the start of the pass CBVs.  These are the last 3 descriptors.
	PassCbvOffset = objCount * gNumberFrameResources;
	GPUParticleCBVOffset = PassCbvOffset + (1 * gNumberFrameResources);

	D3D12_DESCRIPTOR_HEAP_DESC objCBVHeapDesc;
	objCBVHeapDesc.NumDescriptors = objNumberDescriptors;
	objCBVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	objCBVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	objCBVHeapDesc.NodeMask = 0;
	ThrowIfFailed(Device->CreateDescriptorHeap(&objCBVHeapDesc,
		IID_PPV_ARGS(&CBVHeap)));

	// build constant buffer heap for materials
	UINT matCount = (UINT)Materials.size();

	UINT matNumberDescriptors = matCount * gNumberFrameResources;

	D3D12_DESCRIPTOR_HEAP_DESC matCBVHeapDesc;
	matCBVHeapDesc.NumDescriptors = matNumberDescriptors;
	matCBVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	matCBVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	matCBVHeapDesc.NodeMask = 0;
	ThrowIfFailed(Device->CreateDescriptorHeap(&matCBVHeapDesc,
		IID_PPV_ARGS(&matCBVHeap)));

	// build SRV heap for textures
	UINT textureCount = (UINT)Textures.size() + (UINT)CubeMapTextures.size();

	D3D12_DESCRIPTOR_HEAP_DESC SRVHeapDesc;
	SRVHeapDesc.NumDescriptors = textureCount;
	SRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	SRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	SRVHeapDesc.NodeMask = 0;
	ThrowIfFailed(Device->CreateDescriptorHeap(&SRVHeapDesc, IID_PPV_ARGS(&SRVHeap)));

	// heap for GPU particle resources
	D3D12_DESCRIPTOR_HEAP_DESC GPUSRVUAVHeapDesc = {};
	GPUSRVUAVHeapDesc.NumDescriptors = GPUParticleResources.size() * 2;
	GPUSRVUAVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	GPUSRVUAVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(Device->CreateDescriptorHeap(&GPUSRVUAVHeapDesc, IID_PPV_ARGS(&GPUParticleSRVUAVHeap)));
}

void Game::BuildConstantBufferViews()
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	UINT objCount = (UINT)allEntities.size();

	// Need a CBV descriptor for each object for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumberFrameResources; ++frameIndex)
	{
		auto objectCB = FrameResources[frameIndex]->ObjectCB->Resource();
		for (UINT i = 0; i < objCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * objCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = frameIndex * objCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, CBVSRVUAVDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			Device->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	// Last three descriptors are the pass CBVs for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumberFrameResources; ++frameIndex)
	{
		auto passCB = FrameResources[frameIndex]->PassCB->Resource();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		// Offset to the pass cbv in the descriptor heap.
		int heapIndex = PassCbvOffset + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, CBVSRVUAVDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passCBByteSize;

		Device->CreateConstantBufferView(&cbvDesc, handle);
	}

	UINT gpuParticleCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(GPUParticleConstants));

	// Last three descriptors are the particle CBVs for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumberFrameResources; ++frameIndex)
	{
		auto particleCB = FrameResources[frameIndex]->GPUParticleCB->Resource();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = particleCB->GetGPUVirtualAddress();

		// Offset to the pass cbv in the descriptor heap.
		int heapIndex = GPUParticleCBVOffset + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, CBVSRVUAVDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = gpuParticleCBByteSize;

		Device->CreateConstantBufferView(&cbvDesc, handle);
	}

	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
	UINT matCount = (UINT)Materials.size();

	// Last three descriptors are the pass CBVs for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumberFrameResources; ++frameIndex)
	{
		auto matCB = FrameResources[frameIndex]->MaterialCB->Resource();
		for (UINT i = 0; i < matCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = matCB->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * matCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = frameIndex * matCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(matCBVHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, CBVSRVUAVDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc;
			matCBVDesc.BufferLocation = cbAddress;
			matCBVDesc.SizeInBytes = objCBByteSize;

			Device->CreateConstantBufferView(&matCBVDesc, handle);
		}
	}

	int SRVHeapIndex = 0;
	// creating descriptors for each SRV
	for (auto it = Textures.begin(); it != Textures.end(); ++it)
	{
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(SRVHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(SRVHeapIndex, CBVSRVUAVDescriptorSize);

		auto texture = it->second->Resource;

		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVDesc.Format = texture->GetDesc().Format;
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MostDetailedMip = 0;
		SRVDesc.Texture2D.MipLevels = texture->GetDesc().MipLevels;
		SRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		Device->CreateShaderResourceView(texture.Get(), &SRVDesc, handle);

		SRVHeapIndex++;
	}

	for (auto it = CubeMapTextures.begin(); it != CubeMapTextures.end(); ++it)
	{
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(SRVHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(SRVHeapIndex, CBVSRVUAVDescriptorSize);

		auto texture = it->second->Resource;

		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVDesc.Format = texture->GetDesc().Format;
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		SRVDesc.Texture2D.MostDetailedMip = 0;
		SRVDesc.Texture2D.MipLevels = texture->GetDesc().MipLevels;
		SRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		Device->CreateShaderResourceView(texture.Get(), &SRVDesc, handle);

		SRVHeapIndex++;
	}

	UINT GPUResourceIndex = 0;
	for (auto it = GPUParticleResources.begin(); it != GPUParticleResources.end(); ++it)
	{
		auto texture = it->second.get();
		UINT64 resourceSize = texture->StructByStride * texture->NumberOfElements;
		UINT64 counterBufferOffset = AlignForUavCounter(resourceSize);

		ThrowIfFailed(Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(counterBufferOffset + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&texture->Resource)));
		std::wstring w = std::wstring(texture->Name.begin(), texture->Name.end());
		texture->Resource->SetName(w.c_str());

		ThrowIfFailed(Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(counterBufferOffset + sizeof(UINT)),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&texture->UploadHeap)));

		D3D12_UNORDERED_ACCESS_VIEW_DESC resourceUAVDescription = {};
		resourceUAVDescription.Format = DXGI_FORMAT_UNKNOWN;
		resourceUAVDescription.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		resourceUAVDescription.Buffer.FirstElement = 0;
		resourceUAVDescription.Buffer.NumElements = texture->NumberOfElements;
		resourceUAVDescription.Buffer.StructureByteStride = texture->StructByStride;
		resourceUAVDescription.Buffer.CounterOffsetInBytes = counterBufferOffset;

		texture->CPUUAVHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(GPUParticleSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), GPUResourceIndex, CBVSRVUAVDescriptorSize);
		texture->GPUAVHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(GPUParticleSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), GPUResourceIndex, CBVSRVUAVDescriptorSize);
		Device->CreateUnorderedAccessView(texture->Resource.Get(), texture->Resource.Get(), &resourceUAVDescription, texture->CPUUAVHandle);

		GPUResourceIndex++;

		D3D12_SHADER_RESOURCE_VIEW_DESC resourceSRVDescription = {};
		resourceSRVDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		resourceSRVDescription.Format = DXGI_FORMAT_UNKNOWN;
		resourceSRVDescription.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		resourceSRVDescription.Buffer.FirstElement = 0;
		resourceSRVDescription.Buffer.NumElements = texture->NumberOfElements;
		resourceSRVDescription.Buffer.StructureByteStride = texture->StructByStride;

		texture->CPUSRVHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(GPUParticleSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), GPUResourceIndex, CBVSRVUAVDescriptorSize);
		texture->GPUSRVHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(GPUParticleSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), GPUResourceIndex, CBVSRVUAVDescriptorSize);
		Device->CreateShaderResourceView(texture->Resource.Get(), &resourceSRVDescription, texture->CPUSRVHandle);

		GPUResourceIndex++;
	}
}

void Game::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE cbvTable0;
	cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE cbvTable1;
	cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

	CD3DX12_DESCRIPTOR_RANGE cbvTable2;
	cbvTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2);

	CD3DX12_DESCRIPTOR_RANGE cbvTable3;
	cbvTable3.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 3);

	CD3DX12_DESCRIPTOR_RANGE srvTable0;
	srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	// gpu partilce
	CD3DX12_DESCRIPTOR_RANGE srvTable1;
	srvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

	CD3DX12_DESCRIPTOR_RANGE srvTable2;
	srvTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

	CD3DX12_DESCRIPTOR_RANGE uavTable0;
	uavTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE uavTable1;
	uavTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);

	CD3DX12_DESCRIPTOR_RANGE uavTable2;
	uavTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);

	CD3DX12_DESCRIPTOR_RANGE uavTable3;
	uavTable3.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[11];

	// Create root CBVs.
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
	slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);
	slotRootParameter[2].InitAsDescriptorTable(1, &cbvTable2);
	slotRootParameter[3].InitAsDescriptorTable(1, &srvTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[4].InitAsDescriptorTable(1, &cbvTable3); // gpu particle constants
	slotRootParameter[5].InitAsDescriptorTable(1, &srvTable1);
	slotRootParameter[6].InitAsDescriptorTable(1, &srvTable2);
	slotRootParameter[7].InitAsDescriptorTable(1, &uavTable0);
	slotRootParameter[8].InitAsDescriptorTable(1, &uavTable1);
	slotRootParameter[9].InitAsDescriptorTable(1, &uavTable2);
	slotRootParameter[10].InitAsDescriptorTable(1, &uavTable3);
	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(11, slotRootParameter,
		(UINT)staticSamplers.size(),
		staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(Device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(rootSignature.GetAddressOf())));

	// gpu particle command signature
	{
		D3D12_INDIRECT_ARGUMENT_DESC Args[1];
		Args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

		D3D12_COMMAND_SIGNATURE_DESC particleCommandSingatureDescription = {};
		particleCommandSingatureDescription.ByteStride = 36;
		particleCommandSingatureDescription.NumArgumentDescs = 1;
		particleCommandSingatureDescription.pArgumentDescs = Args;

		ThrowIfFailed(Device->CreateCommandSignature(
			&particleCommandSingatureDescription,
			NULL,
			IID_PPV_ARGS(particleCommandSignature.GetAddressOf())));
	}
}

void Game::BuildShadersAndInputLayout()
{
	Shaders["VS"] = d3dUtil::CompileShader(L"Resources/Shaders/VertexShader.hlsl", nullptr, "main", "vs_5_1");
	Shaders["PS"] = d3dUtil::CompileShader(L"Resources/Shaders/PixelShader.hlsl", nullptr, "main", "ps_5_1");

	Shaders["ParticleVS"] = d3dUtil::CompileShader(L"Resources/Shaders/ParticleVS.hlsl", nullptr, "main", "vs_5_1");
	Shaders["ParticlePS"] = d3dUtil::CompileShader(L"Resources/Shaders/ParticlePS.hlsl", nullptr, "main", "ps_5_1");

	Shaders["SkyVS"] = d3dUtil::CompileShader(L"Resources/Shaders/SkyVS.hlsl", nullptr, "main", "vs_5_1");
	Shaders["SkyPS"] = d3dUtil::CompileShader(L"Resources/Shaders/SkyPS.hlsl", nullptr, "main", "ps_5_1");

	Shaders["GPUParticleVS"] = d3dUtil::CompileShader(L"Resources/Shaders/GPUParticleVertexShader.hlsl", nullptr, "main", "vs_5_1");
	Shaders["GPUParticleGS"] = d3dUtil::CompileShader(L"Resources/Shaders/GPUParticleGeometryShader.hlsl", nullptr, "main", "gs_5_1");
	Shaders["GPUParticlePS"] = d3dUtil::CompileShader(L"Resources/Shaders/GPUParticlePixelShader.hlsl", nullptr, "main", "ps_5_1");
	Shaders["GPUParticleEmitCS"] = d3dUtil::CompileShader(L"Resources/Shaders/EmitComputeShader.hlsl", nullptr, "main", "cs_5_1");
	Shaders["GPUParticleUpdateCS"] = d3dUtil::CompileShader(L"Resources/Shaders/UpdateComputeShader.hlsl", nullptr, "main", "cs_5_1");
	Shaders["GPUParticleCopyDrawCountCS"] = d3dUtil::CompileShader(L"Resources/Shaders/CopyDrawCountComputeShader.hlsl", nullptr, "main", "cs_5_1");
	Shaders["GPUParticleDeadListInitCS"] = d3dUtil::CompileShader(L"Resources/Shaders/DeadListInitComputeShader.hlsl", nullptr, "main", "cs_5_1");

	inputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	particleInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SIZE", 0, DXGI_FORMAT_R32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void Game::BuildGeometry()
{
	systemData->LoadOBJFile("Resources/Models/Gun.obj", Device, "Player");
	SubmeshGeometry playerSubMesh = systemData->GetSubSystem("Player");

	systemData->LoadOBJFile("Resources/Models/cube.obj", Device, "box1");
	SubmeshGeometry box1SubMesh = systemData->GetSubSystem("box1");

	systemData->LoadOBJFile("Resources/Models/cylinder.obj", Device, "cylinder");
	SubmeshGeometry cylinderSubMesh = systemData->GetSubSystem("cylinder");

	const uint16_t systemDataVertexSize = systemData->GetCurrentBaseVertexLocation();
	const uint16_t systemDataIndexSize = systemData->GetCurrentBaseIndexLocation();

	std::vector<Vertex> vertices(systemDataVertexSize);
	std::vector<std::uint16_t> indices(systemDataIndexSize);
	UINT k = 0;

 	const XMFLOAT3* systemPositions = systemData->GetPositions();
	const XMFLOAT3* systemNormals = systemData->GetNormals();
	const XMFLOAT3* systemUvs = systemData->GetUVs();

	const uint16_t* systemIndices = systemData->GetIndices();

	for (size_t i = 0; i < systemDataVertexSize; ++i, ++k)
	{
		vertices[k].Position = systemPositions[i];
		vertices[k].Normal = systemNormals[i];
		vertices[k].UV = XMFLOAT2(systemUvs[i].x, systemUvs[i].y);
	}

	k = 0;
	for (size_t i = 0; i < systemDataIndexSize; ++i, ++k)
	{
		indices[k] = systemIndices[i];
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(Device.Get(),
		CommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(Device.Get(),
		CommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["Player"] = playerSubMesh;
	geo->DrawArgs["Box"] = box1SubMesh;
	geo->DrawArgs["Cylinder"] = cylinderSubMesh;

	Geometries[geo->Name] = std::move(geo);

	const UINT emitterVBSize = player->GetEmitter()->GetMaxParticles() * 4 * sizeof(ParticleVertex);
	const UINT emitterIBSize = player->GetEmitter()->GetMaxParticles() * 6 * sizeof(uint16_t);

	auto emitterGeo = std::make_unique<MeshGeometry>();
	emitterGeo->Name = "emitterGeo";

	emitterGeo->VertexBufferCPU = nullptr;
	emitterGeo->VertexBufferGPU = nullptr;

	ThrowIfFailed(D3DCreateBlob(emitterIBSize, &emitterGeo->IndexBufferCPU));
	CopyMemory(emitterGeo->IndexBufferCPU->GetBufferPointer(), player->GetEmitter()->GetParticleIndices(), emitterIBSize);
	
	emitterGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(Device.Get(),
		CommandList.Get(), player->GetEmitter()->GetParticleIndices(), emitterIBSize, emitterGeo->IndexBufferUploader);

	emitterGeo->VertexByteStride = sizeof(ParticleVertex);
	emitterGeo->VertexBufferByteSize = emitterVBSize;
	emitterGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	emitterGeo->IndexBufferByteSize = emitterIBSize;

	SubmeshGeometry subMesh;
	subMesh.IndexCount = player->GetEmitter()->GetMaxParticles() * 6;
	subMesh.StartIndexLocation = 0;
	subMesh.BaseVertexLocation = 0;

	emitterGeo->DrawArgs["emitter"] = subMesh;

	Geometries[emitterGeo->Name] = std::move(emitterGeo);
}

void Game::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePSODescription;
	ZeroMemory(&opaquePSODescription, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePSODescription.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
	opaquePSODescription.pRootSignature = rootSignature.Get();
	opaquePSODescription.VS =
	{
		reinterpret_cast<BYTE*>(Shaders["VS"]->GetBufferPointer()),
		Shaders["VS"]->GetBufferSize()
	};
	opaquePSODescription.PS =
	{
		reinterpret_cast<BYTE*>(Shaders["PS"]->GetBufferPointer()),
		Shaders["PS"]->GetBufferSize()
	};
	opaquePSODescription.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePSODescription.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	opaquePSODescription.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePSODescription.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePSODescription.SampleMask = UINT_MAX;
	opaquePSODescription.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePSODescription.NumRenderTargets = 1;
	opaquePSODescription.RTVFormats[0] = BackBufferFormat;
	opaquePSODescription.SampleDesc.Count = xMsaaState ? 4 : 1;
	opaquePSODescription.SampleDesc.Quality = xMsaaState ? (xMsaaQuality - 1) : 0;
	opaquePSODescription.DSVFormat = DepthStencilFormat;
	ThrowIfFailed(Device->CreateGraphicsPipelineState(&opaquePSODescription, IID_PPV_ARGS(&PSOs["opaque"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC particlePSODescription;
	ZeroMemory(&particlePSODescription, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	particlePSODescription.InputLayout = { particleInputLayout.data(), (UINT)particleInputLayout.size() };
	particlePSODescription.pRootSignature = rootSignature.Get();
	particlePSODescription.VS =
	{
		reinterpret_cast<BYTE*>(Shaders["ParticleVS"]->GetBufferPointer()),
		Shaders["ParticleVS"]->GetBufferSize()
	};
	particlePSODescription.PS =
	{
		reinterpret_cast<BYTE*>(Shaders["ParticlePS"]->GetBufferPointer()),
		Shaders["ParticlePS"]->GetBufferSize()
	};

	CD3DX12_DEPTH_STENCIL_DESC particleDSS(D3D12_DEFAULT);
	particleDSS.DepthEnable = true;
	particleDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	particleDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	CD3DX12_BLEND_DESC particleBlendState(D3D12_DEFAULT);
	particleBlendState.AlphaToCoverageEnable = false;
	particleBlendState.IndependentBlendEnable = false;
	particleBlendState.RenderTarget[0].BlendEnable = true;
	particleBlendState.RenderTarget[0].BlendOp =
		particleBlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	particleBlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	particleBlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	particleBlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	particleBlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	particleBlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
	particleBlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	particlePSODescription.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	particlePSODescription.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	particlePSODescription.BlendState = particleBlendState;
	particlePSODescription.DepthStencilState = particleDSS;
	particlePSODescription.SampleMask = UINT_MAX;
	particlePSODescription.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	particlePSODescription.NumRenderTargets = 1;
	particlePSODescription.RTVFormats[0] = BackBufferFormat;
	particlePSODescription.SampleDesc.Count = xMsaaState ? 4 : 1;
	particlePSODescription.SampleDesc.Quality = xMsaaState ? (xMsaaQuality - 1) : 0;
	particlePSODescription.DSVFormat = DepthStencilFormat;
	ThrowIfFailed(Device->CreateGraphicsPipelineState(&particlePSODescription, IID_PPV_ARGS(&PSOs["emitter"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPSODescription;
	ZeroMemory(&skyPSODescription, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	skyPSODescription.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
	skyPSODescription.pRootSignature = rootSignature.Get();
	skyPSODescription.VS =
	{
		reinterpret_cast<BYTE*>(Shaders["SkyVS"]->GetBufferPointer()),
		Shaders["SkyVS"]->GetBufferSize()
	};
	skyPSODescription.PS =
	{
		reinterpret_cast<BYTE*>(Shaders["SkyPS"]->GetBufferPointer()),
		Shaders["SkyPS"]->GetBufferSize()
	};
	skyPSODescription.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	skyPSODescription.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	skyPSODescription.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	skyPSODescription.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	skyPSODescription.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	skyPSODescription.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	skyPSODescription.SampleMask = UINT_MAX;
	skyPSODescription.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	skyPSODescription.NumRenderTargets = 1;
	skyPSODescription.RTVFormats[0] = BackBufferFormat;
	skyPSODescription.SampleDesc.Count = xMsaaState ? 4 : 1;
	skyPSODescription.SampleDesc.Quality = xMsaaState ? (xMsaaQuality - 1) : 0;
	skyPSODescription.DSVFormat = DepthStencilFormat;
	ThrowIfFailed(Device->CreateGraphicsPipelineState(&skyPSODescription, IID_PPV_ARGS(&PSOs["sky"])));

	// gpu particle graphic PSO
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpuParticlePSODescription;
	ZeroMemory(&gpuParticlePSODescription, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	gpuParticlePSODescription.pRootSignature = rootSignature.Get();
	gpuParticlePSODescription.VS =
	{
		reinterpret_cast<BYTE*>(Shaders["GPUParticleVS"]->GetBufferPointer()),
		Shaders["GPUParticleVS"]->GetBufferSize()
	};
	gpuParticlePSODescription.PS =
	{
		reinterpret_cast<BYTE*>(Shaders["GPUParticlePS"]->GetBufferPointer()),
		Shaders["GPUParticlePS"]->GetBufferSize()
	};
	gpuParticlePSODescription.GS =
	{
		reinterpret_cast<BYTE*>(Shaders["GPUParticleGS"]->GetBufferPointer()),
		Shaders["GPUParticleGS"]->GetBufferSize()
	};

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc = {};
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_ONE;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_DEPTH_STENCIL_DESC depth = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depth.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	depth.DepthEnable = true;

	gpuParticlePSODescription.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gpuParticlePSODescription.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gpuParticlePSODescription.BlendState.RenderTarget[0] = transparencyBlendDesc;
	gpuParticlePSODescription.DepthStencilState = depth;
	gpuParticlePSODescription.SampleMask = UINT_MAX;
	gpuParticlePSODescription.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	gpuParticlePSODescription.NumRenderTargets = 1;
	gpuParticlePSODescription.RTVFormats[0] = BackBufferFormat;
	gpuParticlePSODescription.SampleDesc.Count = xMsaaState ? 4 : 1;
	gpuParticlePSODescription.SampleDesc.Quality = xMsaaState ? (xMsaaQuality - 1) : 0;
	gpuParticlePSODescription.DSVFormat = DepthStencilFormat;
	ThrowIfFailed(Device->CreateGraphicsPipelineState(&gpuParticlePSODescription, IID_PPV_ARGS(&PSOs["GPUParticle"])));

	// gpu particle compute PSOs
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC particleEmitPSO = {};
		particleEmitPSO.pRootSignature = rootSignature.Get();
		particleEmitPSO.CS =
		{
			reinterpret_cast<BYTE*>(Shaders["GPUParticleEmitCS"]->GetBufferPointer()),
			Shaders["GPUParticleEmitCS"]->GetBufferSize()
		};
		particleEmitPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		ThrowIfFailed(Device->CreateComputePipelineState(&particleEmitPSO, IID_PPV_ARGS(&PSOs["particleEmit"])));

		D3D12_COMPUTE_PIPELINE_STATE_DESC particleUpdatePSO = {};
		particleUpdatePSO.pRootSignature = rootSignature.Get();
		particleUpdatePSO.CS =
		{
			reinterpret_cast<BYTE*>(Shaders["GPUParticleUpdateCS"]->GetBufferPointer()),
			Shaders["GPUParticleUpdateCS"]->GetBufferSize()
		};
		particleUpdatePSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		ThrowIfFailed(Device->CreateComputePipelineState(&particleUpdatePSO, IID_PPV_ARGS(&PSOs["particleUpdate"])));

		D3D12_COMPUTE_PIPELINE_STATE_DESC particleDrawPSO = {};
		particleDrawPSO.pRootSignature = rootSignature.Get();
		particleDrawPSO.CS =
		{
			reinterpret_cast<BYTE*>(Shaders["GPUParticleCopyDrawCountCS"]->GetBufferPointer()),
			Shaders["GPUParticleCopyDrawCountCS"]->GetBufferSize()
		};
		particleDrawPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		ThrowIfFailed(Device->CreateComputePipelineState(&particleDrawPSO, IID_PPV_ARGS(&PSOs["particleDraw"])));

		D3D12_COMPUTE_PIPELINE_STATE_DESC particleDeadListPSO = {};
		particleDeadListPSO.pRootSignature = rootSignature.Get();
		particleDeadListPSO.CS =
		{
			reinterpret_cast<BYTE*>(Shaders["GPUParticleDeadListInitCS"]->GetBufferPointer()),
			Shaders["GPUParticleDeadListInitCS"]->GetBufferSize()
		};
		particleDeadListPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		ThrowIfFailed(Device->CreateComputePipelineState(&particleDeadListPSO, IID_PPV_ARGS(&PSOs["particleDeadList"])));
	}
}

void Game::BuildFrameResources()
{
	for (int i = 0; i < gNumberFrameResources; ++i)
	{
		FrameResources.push_back(std::make_unique<FrameResource>(Device.Get(),
			1, (UINT)allEntities.size(), Materials.size(), player->GetEmitter()->GetMaxParticles() * 4, 1));
	}
}

void Game::BuildMaterials()
{
	auto demo1Material = std::make_unique<Material>();
	demo1Material->Name = "demo1";
	demo1Material->MatCBIndex = 0;
	demo1Material->DiffuseSrvHeapIndex = 0;
	demo1Material->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	demo1Material->FresnelR0 = XMFLOAT3(0.5f, 0.5f, 0.5f);
	demo1Material->Roughness = 0.2f;

	Materials[demo1Material->Name] = std::move(demo1Material);

	auto demo2Material = std::make_unique<Material>();
	demo2Material->Name = "demo2";
	demo2Material->MatCBIndex = 1;
	demo2Material->DiffuseSrvHeapIndex = 1;
	demo2Material->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	demo2Material->FresnelR0 = XMFLOAT3(0.5f, 0.5f, 0.5f);
	demo2Material->Roughness = 0.2f;

	Materials[demo2Material->Name] = std::move(demo2Material);

	auto emitterMaterial = std::make_unique<Material>();
	emitterMaterial->Name = "emitter";
	emitterMaterial->MatCBIndex = 2;
	emitterMaterial->DiffuseSrvHeapIndex = 2;
	emitterMaterial->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	emitterMaterial->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	emitterMaterial->Roughness = 1.0f;

	Materials[emitterMaterial->Name] = std::move(emitterMaterial);

	auto skyMaterial = std::make_unique<Material>();
	skyMaterial->Name = "sky";
	skyMaterial->MatCBIndex = 3;
	skyMaterial->DiffuseSrvHeapIndex = 3;
	skyMaterial->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	skyMaterial->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	skyMaterial->Roughness = 1.0f;

	Materials[skyMaterial->Name] = std::move(skyMaterial);
}

void Game::BuildEntities()
{
	int currentEntityIndex = 0;
	int currentObjCBIndex = 0;

	auto gpuEmitterEntity = std::make_unique<Entity>();
	gpuEmitterEntity->SystemWorldIndex = currentEntityIndex;
	systemData->SetScale(currentEntityIndex, 1.0f, 1.0f, 1.0f);
	systemData->SetRotation(currentEntityIndex, 0, 0, 0);
	systemData->SetTranslation(currentEntityIndex, -100.0f, 0.0f, 0.0f);
	systemData->SetWorldMatrix(currentEntityIndex);
	gpuEmitterEntity->ObjCBIndex = currentObjCBIndex;
	allEntities.push_back(std::move(gpuEmitterEntity));
	currentEntityIndex++;
	currentObjCBIndex++;

	auto playerEntity = std::make_unique<Entity>();
	playerEntity->SystemWorldIndex = currentEntityIndex;
	systemData->SetScale(currentEntityIndex, 1.0f, 1.0f, 1.0f);
	systemData->SetRotation(currentEntityIndex, 0, 0, 0);
	systemData->SetTranslation(currentEntityIndex, 3.0f, 2.0f, 0.0f);
	systemData->SetWorldMatrix(currentEntityIndex);
	playerEntity->ObjCBIndex = currentObjCBIndex;
	playerEntity->Geo = Geometries["shapeGeo"].get();
	playerEntity->Mat = Materials["demo2"].get();
	playerEntity->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	playerEntity->meshData = playerEntity->Geo->DrawArgs["Player"];
	allEntities.push_back(std::move(playerEntity));
	playerEntities.push_back(allEntities[currentEntityIndex].get());
	currentEntityIndex++;
	currentObjCBIndex++;

	// scene entitites
	{
		auto sceneEntity1 = std::make_unique<Entity>();
		sceneEntity1->SystemWorldIndex = currentEntityIndex;
		systemData->SetTranslation(currentEntityIndex, 0.0f, 0.0f, 0.0f);
		systemData->SetScale(currentEntityIndex, 20.0f, 0.25f, 20.0f);
		systemData->SetWorldMatrix(currentEntityIndex);
		sceneEntity1->ObjCBIndex = currentObjCBIndex;
		sceneEntity1->Geo = Geometries["shapeGeo"].get();
		sceneEntity1->Mat = Materials["demo1"].get();
		sceneEntity1->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		sceneEntity1->meshData = sceneEntity1->Geo->DrawArgs["Box"];
		allEntities.push_back(std::move(sceneEntity1));
		sceneEntities.push_back(allEntities[currentEntityIndex].get());
		currentEntityIndex++;
		currentObjCBIndex++;

		auto sceneEntity2 = std::make_unique<Entity>();
		sceneEntity2->SystemWorldIndex = currentEntityIndex;
		systemData->SetTranslation(currentEntityIndex, 25.0f, 0.0f, 5.0f);
		systemData->SetScale(currentEntityIndex, 30.0f, 0.25f, 5.0f);
		systemData->SetWorldMatrix(currentEntityIndex);
		sceneEntity2->ObjCBIndex = currentObjCBIndex;
		sceneEntity2->Geo = Geometries["shapeGeo"].get();
		sceneEntity2->Mat = Materials["demo2"].get();
		sceneEntity2->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		sceneEntity2->meshData = sceneEntity2->Geo->DrawArgs["Box"];
		allEntities.push_back(std::move(sceneEntity2));
		sceneEntities.push_back(allEntities[currentEntityIndex].get());
		currentEntityIndex++;
		currentObjCBIndex++;

		auto sceneEntity3 = std::make_unique<Entity>();
		sceneEntity3->SystemWorldIndex = currentEntityIndex;
		systemData->SetTranslation(currentEntityIndex, 60.0f, 0.0f, 15.0f);
		systemData->SetScale(currentEntityIndex, 40.0f, 0.25f, 40.0f);
		systemData->SetWorldMatrix(currentEntityIndex);
		sceneEntity3->ObjCBIndex = currentObjCBIndex;
		sceneEntity3->Geo = Geometries["shapeGeo"].get();
		sceneEntity3->Mat = Materials["demo2"].get();
		sceneEntity3->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		sceneEntity3->meshData = sceneEntity3->Geo->DrawArgs["Box"];
		allEntities.push_back(std::move(sceneEntity3));
		sceneEntities.push_back(allEntities[currentEntityIndex].get());
		currentEntityIndex++;
		currentObjCBIndex++;

		auto sceneEntity4 = std::make_unique<Entity>();
		sceneEntity4->SystemWorldIndex = currentEntityIndex;
		systemData->SetTranslation(currentEntityIndex, 90.0f, 0.0f, 25.0f);
		systemData->SetScale(currentEntityIndex, 20.0f, 0.25f, 5.0f);
		systemData->SetWorldMatrix(currentEntityIndex);
		sceneEntity4->ObjCBIndex = currentObjCBIndex;
		sceneEntity4->Geo = Geometries["shapeGeo"].get();
		sceneEntity4->Mat = Materials["demo2"].get();
		sceneEntity4->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		sceneEntity4->meshData = sceneEntity4->Geo->DrawArgs["Box"];
		allEntities.push_back(std::move(sceneEntity4));
		sceneEntities.push_back(allEntities[currentEntityIndex].get());
		currentEntityIndex++;
		currentObjCBIndex++;

		auto sceneEntity5 = std::make_unique<Entity>();
		sceneEntity5->SystemWorldIndex = currentEntityIndex;
		systemData->SetTranslation(currentEntityIndex, 110.0f, 0.0f, 20.0f);
		systemData->SetScale(currentEntityIndex, 20.0f, 0.25f, 20.0f);
		systemData->SetWorldMatrix(currentEntityIndex);
		sceneEntity5->ObjCBIndex = currentObjCBIndex;
		sceneEntity5->Geo = Geometries["shapeGeo"].get();
		sceneEntity5->Mat = Materials["demo2"].get();
		sceneEntity5->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		sceneEntity5->meshData = sceneEntity5->Geo->DrawArgs["Box"];
		allEntities.push_back(std::move(sceneEntity5));
		sceneEntities.push_back(allEntities[currentEntityIndex].get());
		currentEntityIndex++;
		currentObjCBIndex++;

		auto sceneEntity6 = std::make_unique<Entity>();
		sceneEntity6->SystemWorldIndex = currentEntityIndex;
		systemData->SetTranslation(currentEntityIndex, 20.0f, 0.0f, 30.0f);
		systemData->SetScale(currentEntityIndex, 10.0f, 0.25f, 10.0f);
		systemData->SetWorldMatrix(currentEntityIndex);
		sceneEntity6->ObjCBIndex = currentObjCBIndex;
		sceneEntity6->Geo = Geometries["shapeGeo"].get();
		sceneEntity6->Mat = Materials["demo2"].get();
		sceneEntity6->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		sceneEntity6->meshData = sceneEntity6->Geo->DrawArgs["Box"];
		allEntities.push_back(std::move(sceneEntity6));
		sceneEntities.push_back(allEntities[currentEntityIndex].get());
		currentEntityIndex++;
		currentObjCBIndex++;

		auto sceneEntity7 = std::make_unique<Entity>();
		sceneEntity7->SystemWorldIndex = currentEntityIndex;
		systemData->SetTranslation(currentEntityIndex, 30.0f, 0.0f, -10.0f);
		systemData->SetScale(currentEntityIndex, 10.0f, 0.25f, 10.0f);
		systemData->SetWorldMatrix(currentEntityIndex);
		sceneEntity7->ObjCBIndex = currentObjCBIndex;
		sceneEntity7->Geo = Geometries["shapeGeo"].get();
		sceneEntity7->Mat = Materials["demo2"].get();
		sceneEntity7->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		sceneEntity7->meshData = sceneEntity7->Geo->DrawArgs["Box"];
		allEntities.push_back(std::move(sceneEntity7));
		sceneEntities.push_back(allEntities[currentEntityIndex].get());
		currentEntityIndex++;
		currentObjCBIndex++;
	}

	// enemy entities
	{
		auto enemyEntity1 = std::make_unique<EnemyEntity>();
		enemyEntity1->SystemWorldIndex = currentEntityIndex;
		systemData->SetScale(currentEntityIndex, 4.0f, 8.0f, 4.0f);
		systemData->SetTranslation(currentEntityIndex, 75.0f, 4.0f, 20.0f);
		systemData->SetWorldMatrix(currentEntityIndex);
		enemyEntity1->ObjCBIndex = currentObjCBIndex;
		enemyEntity1->Geo = Geometries["shapeGeo"].get();
		enemyEntity1->Mat = Materials["demo1"].get();
		enemyEntity1->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		enemyEntity1->meshData = enemyEntity1->Geo->DrawArgs["Cylinder"];
		enemyEntity1->isRanged = false;
		enemyEntity1->currentWaypointIndex = 0;
		allEntities.push_back(std::move(enemyEntity1));
		enemyEntities.push_back((EnemyEntity*)allEntities[currentEntityIndex].get());
		currentEntityIndex++;
		currentObjCBIndex++;

		auto enemyEntity2 = std::make_unique<EnemyEntity>();
		enemyEntity2->SystemWorldIndex = currentEntityIndex;
		systemData->SetScale(currentEntityIndex, 4.0f, 8.0f, 4.0f);
		systemData->SetTranslation(currentEntityIndex, 50.0f, 4.0f, 5.0f);
		systemData->SetWorldMatrix(currentEntityIndex);
		enemyEntity2->ObjCBIndex = currentObjCBIndex;
		enemyEntity2->Geo = Geometries["shapeGeo"].get();
		enemyEntity2->Mat = Materials["demo1"].get();
		enemyEntity2->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		enemyEntity2->meshData = enemyEntity2->Geo->DrawArgs["Cylinder"];
		enemyEntity2->isRanged = false;
		enemyEntity2->currentWaypointIndex = 3;
		allEntities.push_back(std::move(enemyEntity2));
		enemyEntities.push_back((EnemyEntity*)allEntities[currentEntityIndex].get());
		currentEntityIndex++;
		currentObjCBIndex++;

		/*auto enemyEntity3 = std::make_unique<EnemyEntity>();
		enemyEntity3->SystemWorldIndex = currentEntityIndex;
		systemData->SetScale(currentEntityIndex, 2.0f, 4.0f, 1.0f);
		systemData->SetTranslation(currentEntityIndex, 30.0f, 2.0f, -10.0f);
		systemData->SetWorldMatrix(currentEntityIndex);
		enemyEntity3->ObjCBIndex = currentObjCBIndex;
		enemyEntity3->Geo = Geometries["shapeGeo"].get();
		enemyEntity3->Mat = Materials["demo1"].get();
		enemyEntity3->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		enemyEntity3->meshData = enemyEntity3->Geo->DrawArgs["Cylinder"];
		allEntities.push_back(std::move(enemyEntity3));
		enemyEntities.push_back(allEntities[currentEntityIndex].get());
		currentEntityIndex++;
		currentObjCBIndex++;*/

		/*auto enemyEntity4 = std::make_unique<EnemyEntity>();
		enemyEntity4->SystemWorldIndex = currentEntityIndex;
		systemData->SetScale(currentEntityIndex, 2.0f, 4.0f, 1.0f);
		systemData->SetTranslation(currentEntityIndex, 20.0f, 2.0f, -10.0f);
		systemData->SetWorldMatrix(currentEntityIndex);
		enemyEntity4->ObjCBIndex = currentObjCBIndex;
		enemyEntity4->Geo = Geometries["shapeGeo"].get();
		enemyEntity4->Mat = Materials["demo1"].get();
		enemyEntity4->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		enemyEntity4->meshData = enemyEntity4->Geo->DrawArgs["Cylinder"];
		enemyEntity4->isRanged = true;
		enemyEntity4->currentWaypointIndex = 0;
		allEntities.push_back(std::move(enemyEntity4));
		enemyEntities.push_back((EnemyEntity*)allEntities[currentEntityIndex].get());
		currentEntityIndex++;
		currentObjCBIndex++;*/
	}

	// Add waypoint entities here
	{
		auto waypointEntity1 = std::make_unique<Entity>();
		waypointEntity1->SystemWorldIndex = currentEntityIndex;
		systemData->SetScale(currentEntityIndex, 5.0f, 5.0f, 5.0f);
		systemData->SetTranslation(currentEntityIndex, 50.0f, 2.0f, 30.0f);
		systemData->SetWorldMatrix(currentEntityIndex);
		waypointEntity1->ObjCBIndex = currentObjCBIndex;
		waypointEntity1->Geo = Geometries["shapeGeo"].get();
		waypointEntity1->Mat = Materials["demo1"].get();
		waypointEntity1->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		waypointEntity1->meshData = waypointEntity1->Geo->DrawArgs["Box"];
		allEntities.push_back(std::move(waypointEntity1));
		waypointEntities.push_back(allEntities[currentEntityIndex].get());
		currentEntityIndex++;
		currentObjCBIndex++;

		auto waypointEntity2 = std::make_unique<Entity>();
		waypointEntity2->SystemWorldIndex = currentEntityIndex;
		systemData->SetScale(currentEntityIndex, 5.0f, 5.0f, 5.0f);
		systemData->SetTranslation(currentEntityIndex, 50.0f, 2.0f, 0.0f);
		systemData->SetWorldMatrix(currentEntityIndex);
		waypointEntity2->ObjCBIndex = currentObjCBIndex;
		waypointEntity2->Geo = Geometries["shapeGeo"].get();
		waypointEntity2->Mat = Materials["demo1"].get();
		waypointEntity2->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		waypointEntity2->meshData = waypointEntity2->Geo->DrawArgs["Box"];
		allEntities.push_back(std::move(waypointEntity2));
		waypointEntities.push_back(allEntities[currentEntityIndex].get());
		currentEntityIndex++;
		currentObjCBIndex++;

		auto waypointEntity3 = std::make_unique<Entity>();
		waypointEntity3->SystemWorldIndex = currentEntityIndex;
		systemData->SetScale(currentEntityIndex, 5.0f, 5.0f, 5.0f);
		systemData->SetTranslation(currentEntityIndex, 70.0f, 2.0f, 30.0f);
		systemData->SetWorldMatrix(currentEntityIndex);
		waypointEntity3->ObjCBIndex = currentObjCBIndex;
		waypointEntity3->Geo = Geometries["shapeGeo"].get();
		waypointEntity3->Mat = Materials["demo1"].get();
		waypointEntity3->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		waypointEntity3->meshData = waypointEntity3->Geo->DrawArgs["Box"];
		allEntities.push_back(std::move(waypointEntity3));
		waypointEntities.push_back(allEntities[currentEntityIndex].get());
		currentEntityIndex++;
		currentObjCBIndex++;

		auto waypointEntity4 = std::make_unique<Entity>();
		waypointEntity4->SystemWorldIndex = currentEntityIndex;
		systemData->SetScale(currentEntityIndex, 5.0f, 5.0f, 5.0f);
		systemData->SetTranslation(currentEntityIndex, 70.0f, 2.0f, 0.0f);
		systemData->SetWorldMatrix(currentEntityIndex);
		waypointEntity4->ObjCBIndex = currentObjCBIndex;
		waypointEntity4->Geo = Geometries["shapeGeo"].get();
		waypointEntity4->Mat = Materials["demo1"].get();
		waypointEntity4->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		waypointEntity4->meshData = waypointEntity4->Geo->DrawArgs["Box"];
		allEntities.push_back(std::move(waypointEntity4));
		waypointEntities.push_back(allEntities[currentEntityIndex].get());
		currentEntityIndex++;
		currentObjCBIndex++;
	}

	auto emitterEntity = std::make_unique<Entity>();
	emitterEntity->SystemWorldIndex = currentEntityIndex;
	systemData->SetScale(currentEntityIndex, 1.0f, 1.0f, 1.0f);
	systemData->SetTranslation(currentEntityIndex, 0.0f, 0.0f, 0.0f);
	systemData->SetWorldMatrix(currentEntityIndex);
	emitterEntity->ObjCBIndex = currentObjCBIndex;
	emitterEntity->Geo = Geometries["emitterGeo"].get();
	emitterEntity->Mat = Materials["emitter"].get();
	emitterEntity->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	emitterEntity->meshData = emitterEntity->Geo->DrawArgs["emitter"];
	allEntities.push_back(std::move(emitterEntity));
	emitterEntities.push_back(allEntities[currentEntityIndex].get());
	currentEntityIndex++;
	currentObjCBIndex++;

	auto skyEntity = std::make_unique<Entity>();
	skyEntity->SystemWorldIndex = currentEntityIndex;
	systemData->SetScale(currentEntityIndex, 5000.0f, 5000.0f, 5000.0f);
	systemData->SetTranslation(currentEntityIndex, 0.0f, 0.0f, 0.0f);
	systemData->SetWorldMatrix(currentEntityIndex);
	skyEntity->ObjCBIndex = currentObjCBIndex;
	skyEntity->Geo = Geometries["shapeGeo"].get();
	skyEntity->Mat = Materials["sky"].get();
	skyEntity->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skyEntity->meshData = skyEntity->Geo->DrawArgs["Box"];
	allEntities.push_back(std::move(skyEntity));
	skyEntities.push_back(allEntities[currentEntityIndex].get());
	currentEntityIndex++;
	currentObjCBIndex++;
}

void Game::DrawGPUParticles(ID3D12GraphicsCommandList* cmdList)
{
	cmdList->SetPipelineState(PSOs["particleEmit"].Get());
	cmdList->SetComputeRootSignature(rootSignature.Get());

	ID3D12DescriptorHeap* objDescriptorHeaps[] = { CBVHeap.Get() };
	CommandList->SetDescriptorHeaps(_countof(objDescriptorHeaps), objDescriptorHeaps);

	int passCbvIndex = PassCbvOffset + currentFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, CBVSRVUAVDescriptorSize);
	CommandList->SetComputeRootDescriptorTable(1, passCbvHandle);

	int gpuParticleCBVIndex = GPUParticleCBVOffset + currentFrameResourceIndex;
	auto gpuParticleCBVHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVHeap->GetGPUDescriptorHandleForHeapStart());
	gpuParticleCBVHandle.Offset(gpuParticleCBVIndex, CBVSRVUAVDescriptorSize);
	CommandList->SetComputeRootDescriptorTable(4, gpuParticleCBVHandle);

	ID3D12DescriptorHeap* descriptorHeaps[] = { GPUParticleSRVUAVHeap.Get() };
	CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	cmdList->SetComputeRootDescriptorTable(7, GPUParticleResources["ParticlePool"]->GPUAVHandle);
	cmdList->SetComputeRootDescriptorTable(8, GPUParticleResources["DeadList"]->GPUAVHandle);
	cmdList->SetComputeRootDescriptorTable(9, GPUParticleResources["DrawList"]->GPUAVHandle);
	cmdList->SetComputeRootDescriptorTable(10, GPUParticleResources["DrawArgs"]->GPUAVHandle);

	float emitTimeCounter = gpuEmitter->GetEmitTimeCounter();
	float timeBetweenEmit = gpuEmitter->GetTimeBetweenEmit();
	float maxParticles = gpuEmitter->GetMaxParticles();

	while (emitTimeCounter >= timeBetweenEmit)
	{
		int emitCount = emitTimeCounter / timeBetweenEmit;
		emitCount = min(emitCount, 65535);

		gpuEmitter->SetEmitCount(emitCount);
		gpuEmitter->SetEmitTimeCounter(fmod(emitTimeCounter, timeBetweenEmit));

		UpadteGPUParticleCBs(timer);

		cmdList->Dispatch(emitCount, 1, 1);

		emitTimeCounter = gpuEmitter->GetEmitTimeCounter();
		timeBetweenEmit = gpuEmitter->GetTimeBetweenEmit();
	}

	CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GPUParticleResources["DrawList"]->Resource.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST));

	CommandList->CopyResource(GPUParticleResources["DrawList"]->Resource.Get(), GPUParticleResources["DrawList"]->UploadHeap.Get());

	CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GPUParticleResources["DrawList"]->Resource.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	cmdList->SetPipelineState(PSOs["particleUpdate"].Get());
	cmdList->SetComputeRootSignature(rootSignature.Get());
	cmdList->Dispatch(maxParticles, 1, 1);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(GPUParticleResources["DrawList"]->Resource.Get()));

	cmdList->SetPipelineState(PSOs["particleDraw"].Get());
	cmdList->SetComputeRootSignature(rootSignature.Get());
	cmdList->Dispatch(1, 1, 1);

	cmdList->SetPipelineState(PSOs["GPUParticle"].Get());
	cmdList->SetGraphicsRootSignature(rootSignature.Get());

	cmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	ID3D12DescriptorHeap* objDescriptorHeaps1[] = { CBVHeap.Get() };
	CommandList->SetDescriptorHeaps(_countof(objDescriptorHeaps), objDescriptorHeaps);

	UINT objCBVIndex = currentFrameResourceIndex * (UINT)allEntities.size() + allEntities[0].get()->ObjCBIndex;
	auto objCBVHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVHeap->GetGPUDescriptorHandleForHeapStart());
	objCBVHandle.Offset(objCBVIndex, CBVSRVUAVDescriptorSize);

	cmdList->SetGraphicsRootDescriptorTable(0, objCBVHandle);

	int passCbvIndex1 = PassCbvOffset + currentFrameResourceIndex;
	auto passCbvHandle1 = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle1.Offset(passCbvIndex1, CBVSRVUAVDescriptorSize);
	CommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle1);

	int gpuParticleCBVIndex1 = GPUParticleCBVOffset + currentFrameResourceIndex;
	auto gpuParticleCBVHandle1 = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVHeap->GetGPUDescriptorHandleForHeapStart());
	gpuParticleCBVHandle.Offset(gpuParticleCBVIndex1, CBVSRVUAVDescriptorSize);
	CommandList->SetGraphicsRootDescriptorTable(4, gpuParticleCBVHandle1);

	ID3D12DescriptorHeap* descriptorHeaps1[] = { GPUParticleSRVUAVHeap.Get() };
	CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	cmdList->SetGraphicsRootDescriptorTable(5, GPUParticleResources["ParticlePool"]->GPUSRVHandle);
	cmdList->SetGraphicsRootDescriptorTable(6, GPUParticleResources["DrawList"]->GPUSRVHandle);

	CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GPUParticleResources["DrawArgs"]->Resource.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));

	cmdList->ExecuteIndirect(particleCommandSignature.Get(),
		1,
		GPUParticleResources["DrawArgs"]->Resource.Get(),
		0,
		nullptr,
		0);

	CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GPUParticleResources["DrawArgs"]->Resource.Get(),
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
}

void Game::DrawEntities(ID3D12GraphicsCommandList* cmdList, const std::vector<Entity*> entities)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = currentFrameResource->ObjectCB->Resource();
	auto matCB = currentFrameResource->MaterialCB->Resource();

	// For each render item...
	for (size_t i = 0; i < entities.size(); ++i)
	{
		auto e = entities[i];

		cmdList->IASetVertexBuffers(0, 1, &e->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&e->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(e->PrimitiveType);

		ID3D12DescriptorHeap* objDescriptorHeaps[] = { CBVHeap.Get() };
		cmdList->SetDescriptorHeaps(_countof(objDescriptorHeaps), objDescriptorHeaps);

		// Offset to the CBV in the descriptor heap for this object and for this frame resource.
		UINT objCBVIndex = currentFrameResourceIndex * (UINT)allEntities.size() + e->ObjCBIndex;
		auto objCBVHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVHeap->GetGPUDescriptorHandleForHeapStart());
		objCBVHandle.Offset(objCBVIndex, CBVSRVUAVDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(0, objCBVHandle);

		ID3D12DescriptorHeap* matDescriptorHeaps[] = { matCBVHeap.Get() };
		cmdList->SetDescriptorHeaps(_countof(matDescriptorHeaps), matDescriptorHeaps);

		UINT matCBVIndex = currentFrameResourceIndex * (UINT)Materials.size() + e->Mat->MatCBIndex;
		auto matCBVHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(matCBVHeap->GetGPUDescriptorHandleForHeapStart());
		matCBVHandle.Offset(matCBVIndex, CBVSRVUAVDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(2, matCBVHandle);

		ID3D12DescriptorHeap* srvDescriptorHeaps[] = { SRVHeap.Get() };
		cmdList->SetDescriptorHeaps(_countof(srvDescriptorHeaps), srvDescriptorHeaps);

		UINT srvIndex = e->Mat->DiffuseSrvHeapIndex;
		auto srvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(SRVHeap->GetGPUDescriptorHandleForHeapStart());
		srvHandle.Offset(srvIndex, CBVSRVUAVDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(3, srvHandle);

		cmdList->DrawIndexedInstanced(e->meshData.IndexCount, 1, e->meshData.StartIndexLocation, e->meshData.BaseVertexLocation, 0);
	}
}

void Game::DrawEntities(ID3D12GraphicsCommandList* cmdList, const std::vector<EnemyEntity*> entities)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = currentFrameResource->ObjectCB->Resource();
	auto matCB = currentFrameResource->MaterialCB->Resource();

	// For each render item...
	for (size_t i = 0; i < entities.size(); ++i)
	{
		auto e = entities[i];

		cmdList->IASetVertexBuffers(0, 1, &e->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&e->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(e->PrimitiveType);

		ID3D12DescriptorHeap* objDescriptorHeaps[] = { CBVHeap.Get() };
		cmdList->SetDescriptorHeaps(_countof(objDescriptorHeaps), objDescriptorHeaps);

		// Offset to the CBV in the descriptor heap for this object and for this frame resource.
		UINT objCBVIndex = currentFrameResourceIndex * (UINT)allEntities.size() + e->ObjCBIndex;
		auto objCBVHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVHeap->GetGPUDescriptorHandleForHeapStart());
		objCBVHandle.Offset(objCBVIndex, CBVSRVUAVDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(0, objCBVHandle);

		ID3D12DescriptorHeap* matDescriptorHeaps[] = { matCBVHeap.Get() };
		cmdList->SetDescriptorHeaps(_countof(matDescriptorHeaps), matDescriptorHeaps);

		UINT matCBVIndex = currentFrameResourceIndex * (UINT)Materials.size() + e->Mat->MatCBIndex;
		auto matCBVHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(matCBVHeap->GetGPUDescriptorHandleForHeapStart());
		matCBVHandle.Offset(matCBVIndex, CBVSRVUAVDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(2, matCBVHandle);

		ID3D12DescriptorHeap* srvDescriptorHeaps[] = { SRVHeap.Get() };
		cmdList->SetDescriptorHeaps(_countof(srvDescriptorHeaps), srvDescriptorHeaps);

		UINT srvIndex = e->Mat->DiffuseSrvHeapIndex;
		auto srvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(SRVHeap->GetGPUDescriptorHandleForHeapStart());
		srvHandle.Offset(srvIndex, CBVSRVUAVDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(3, srvHandle);

		cmdList->DrawIndexedInstanced(e->meshData.IndexCount, 1, e->meshData.StartIndexLocation, e->meshData.BaseVertexLocation, 0);
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> Game::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}