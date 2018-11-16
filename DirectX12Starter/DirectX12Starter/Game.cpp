#include "Game.h"

const int gNumberFrameResources = 3;

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
}

bool Game::Initialize()
{
	if (!DXCore::Initialize())
		return false;

	// reset the command list to prep for initialization commands
	ThrowIfFailed(CommandList->Reset(CommandListAllocator.Get(), nullptr));

	systemData = new SystemData();

	mainCamera = Camera(screenWidth, screenHeight);

	inputManager = InputManager::getInstance();

	player = new Player();

	enemies = new Enemies();

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

	// execute the initialization commands
	ThrowIfFailed(CommandList->Close());
	ID3D12CommandList* cmdsLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// wait until initialization is complete
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

	player->Update(timer, playerEntities[0]);
	enemies->Update(timer, playerEntities[0], enemyEntities);

	UpdateObjectCBs(timer);
	UpdateMainPassCB(timer);
	UpadteMaterialCBs(timer);
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
	CommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
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
			XMMATRIX world = XMLoadFloat4x4(&e->World);
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
	MainPassCB.ambientLight = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);

	MainPassCB.lights[0].Direction = { 1.0f, -1.0f, 1.0f };
	MainPassCB.lights[0].Strength = { 1.0f, 1.0f, 0.9f };

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

void Game::BuildTextures()
{
	auto demo1Texture = std::make_unique<Texture>();
	demo1Texture->Name = "demo1";
	demo1Texture->Filename = L"Resources/Textures/Demo1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(Device.Get(),
		CommandList.Get(), demo1Texture->Filename.c_str(),
		demo1Texture->Resource,
		demo1Texture->UploadHeap));

	Textures[demo1Texture->Name] = std::move(demo1Texture);

	auto demo2Texture = std::make_unique<Texture>();
	demo2Texture->Name = "demo2";
	demo2Texture->Filename = L"Resources/Textures/Demo2.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(Device.Get(),
		CommandList.Get(), demo2Texture->Filename.c_str(),
		demo2Texture->Resource,
		demo2Texture->UploadHeap));

	Textures[demo2Texture->Name] = std::move(demo2Texture);


	// Skybox Texture
	auto skyTexture = std::make_unique<Texture>();
	skyTexture->Name = "skyboxMap";
	skyTexture->Filename = L"Resources/Textures/grasscube1024.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(Device.Get(),
		CommandList.Get(), skyTexture->Filename.c_str(),
		skyTexture->Resource,
		skyTexture->UploadHeap));

	Textures[skyTexture->Name] = std::move(skyTexture);
}

void Game::BuildDescriptorHeaps()
{
	// build constant buffer heap for objects
	UINT objCount = (UINT)allEntities.size();

	// Need a CBV descriptor for each object for each frame resource,
	// +1 for the perPass CBV for each frame resource.
	UINT objNumberDescriptors = (objCount + 1) * gNumberFrameResources;

	// Save an offset to the start of the pass CBVs.  These are the last 3 descriptors.
	PassCbvOffset = objCount * gNumberFrameResources;

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
	UINT textureCount = (UINT)Textures.size();

	D3D12_DESCRIPTOR_HEAP_DESC SRVHeapDesc;
	SRVHeapDesc.NumDescriptors = textureCount;
	SRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	SRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	SRVHeapDesc.NodeMask = 0;
	ThrowIfFailed(Device->CreateDescriptorHeap(&SRVHeapDesc, IID_PPV_ARGS(&SRVHeap)));

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

		if (it->second->Name == "skyboxMap")
		{
			SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			SRVDesc.TextureCube.MostDetailedMip = 0;
			SRVDesc.TextureCube.MipLevels = texture->GetDesc().MipLevels;
			SRVDesc.TextureCube.ResourceMinLODClamp = 0.0f;
			SRVDesc.Format = texture->GetDesc().Format;
		}
		else
		{
			SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			SRVDesc.Format = texture->GetDesc().Format;
			SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			SRVDesc.Texture2D.MostDetailedMip = 0;
			SRVDesc.Texture2D.MipLevels = texture->GetDesc().MipLevels;
			SRVDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		}

		Device->CreateShaderResourceView(texture.Get(), &SRVDesc, handle);

		SRVHeapIndex++;
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

	CD3DX12_DESCRIPTOR_RANGE srvTable0;
	srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Create root CBVs.
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
	slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);
	slotRootParameter[2].InitAsDescriptorTable(1, &cbvTable2);
	slotRootParameter[3].InitAsDescriptorTable(1, &srvTable0, D3D12_SHADER_VISIBILITY_PIXEL);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, 
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
}

void Game::BuildShadersAndInputLayout()
{
	Shaders["VS"] = d3dUtil::CompileShader(L"Resources/Shaders/VertexShader.hlsl", nullptr, "main", "vs_5_1");
	Shaders["PS"] = d3dUtil::CompileShader(L"Resources/Shaders/PixelShader.hlsl", nullptr, "main", "ps_5_1");

	inputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void Game::BuildGeometry()
{
	systemData->LoadOBJFile("Resources/Models/cube.obj", Device, "box1");
	SubmeshGeometry box1Submesh;
	SubSystem box1SubSystem = systemData->GetSubSystem("box1");
	box1Submesh.IndexCount = box1SubSystem.count;
	box1Submesh.StartIndexLocation = box1SubSystem.baseLocation;
	box1Submesh.BaseVertexLocation = box1SubSystem.baseLocation;

	systemData->LoadOBJFile("Resources/Models/cylinder.obj", Device, "cylinder");
	SubmeshGeometry cylinderSubmesh;
	SubSystem cylinderSubSystem = systemData->GetSubSystem("cylinder");
	cylinderSubmesh.IndexCount = cylinderSubSystem.count;
	cylinderSubmesh.StartIndexLocation = cylinderSubSystem.baseLocation;
	cylinderSubmesh.BaseVertexLocation = cylinderSubSystem.baseLocation;

	const uint16_t systemDataSize = systemData->GetCurrentBaseLocation();

	std::vector<Vertex> vertices(systemDataSize);
	std::vector<std::uint16_t> indices(systemDataSize);
	UINT k = 0;

	const XMFLOAT3* systemPositions = systemData->GetPositions();
	const XMFLOAT3* systemNormals = systemData->GetNormals();
	const XMFLOAT2* systemUvs = systemData->GetUvs();

	const uint16_t* systemIndices = systemData->GetIndices();

	for (size_t i = 0; i < systemDataSize; ++i, ++k)
	{
		vertices[k].Position = systemPositions[i];
		vertices[k].Normal = systemNormals[i];
		vertices[k].UV = systemUvs[i];
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

	geo->DrawArgs["box1"] = box1Submesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	Geometries[geo->Name] = std::move(geo);
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
}

void Game::BuildFrameResources()
{
	for (int i = 0; i < gNumberFrameResources; ++i)
	{
		FrameResources.push_back(std::make_unique<FrameResource>(Device.Get(),
			1, (UINT)allEntities.size(), Materials.size()));
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
}

void Game::BuildEntities()
{
	int currentEntityIndex = 0;
	int currentObjCBIndex = 0;

	auto playerEntity = std::make_unique<Entity>();
	playerEntity->SetScale(1.0f, 4.0f, 1.0f);
	playerEntity->SetTranslation(3.0f, 2.0f, 0.0f);
	playerEntity->SetWorldMatrix();
	playerEntity->ObjCBIndex = currentObjCBIndex;
	playerEntity->Geo = Geometries["shapeGeo"].get();
	playerEntity->Mat = Materials["demo2"].get();
	playerEntity->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	playerEntity->IndexCount = playerEntity->Geo->DrawArgs["cylinder"].IndexCount;
	playerEntity->StartIndexLocation = playerEntity->Geo->DrawArgs["cylinder"].StartIndexLocation;
	playerEntity->BaseVertexLocation = playerEntity->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	allEntities.push_back(std::move(playerEntity));
	playerEntities.push_back(allEntities[currentEntityIndex].get());
	currentEntityIndex++;
	currentObjCBIndex++;

	auto sceneEntity1 = std::make_unique<Entity>();
	sceneEntity1->SetScale(20.0f, 0.25f, 20.0f);
	sceneEntity1->SetWorldMatrix();
	sceneEntity1->ObjCBIndex = currentObjCBIndex;
	sceneEntity1->Geo = Geometries["shapeGeo"].get();
	sceneEntity1->Mat = Materials["demo1"].get();
	sceneEntity1->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	sceneEntity1->IndexCount = sceneEntity1->Geo->DrawArgs["box1"].IndexCount;
	sceneEntity1->StartIndexLocation = sceneEntity1->Geo->DrawArgs["box1"].StartIndexLocation;
	sceneEntity1->BaseVertexLocation = sceneEntity1->Geo->DrawArgs["box1"].BaseVertexLocation;
	allEntities.push_back(std::move(sceneEntity1));
	sceneEntities.push_back(allEntities[currentEntityIndex].get());
	currentEntityIndex++;
	currentObjCBIndex++;

	auto sceneEntity2 = std::make_unique<Entity>();
	sceneEntity2->SetTranslation(20.0f, 0.0f, 0.0f);
	sceneEntity2->SetScale(20.0f, 0.25f, 20.0f);
	sceneEntity2->SetWorldMatrix();
	sceneEntity2->ObjCBIndex = currentObjCBIndex;
	sceneEntity2->Geo = Geometries["shapeGeo"].get();
	sceneEntity2->Mat = Materials["demo2"].get();
	sceneEntity2->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	sceneEntity2->IndexCount = sceneEntity2->Geo->DrawArgs["box1"].IndexCount;
	sceneEntity2->StartIndexLocation = sceneEntity2->Geo->DrawArgs["box1"].StartIndexLocation;
	sceneEntity2->BaseVertexLocation = sceneEntity2->Geo->DrawArgs["box1"].BaseVertexLocation;
	allEntities.push_back(std::move(sceneEntity2));
	sceneEntities.push_back(allEntities[currentEntityIndex].get());
	currentEntityIndex++;
	currentObjCBIndex++;

	auto enemyEntity1 = std::make_unique<Entity>();
	enemyEntity1->SetTranslation(18.0f, 1.0f, 8.0f);
	enemyEntity1->SetScale(1.0f, 2.0f, 1.0f);
	enemyEntity1->SetWorldMatrix();
	enemyEntity1->ObjCBIndex = currentObjCBIndex;
	enemyEntity1->Geo = Geometries["shapeGeo"].get();
	enemyEntity1->Mat = Materials["demo1"].get();
	enemyEntity1->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	enemyEntity1->IndexCount = enemyEntity1->Geo->DrawArgs["cylinder"].IndexCount;
	enemyEntity1->StartIndexLocation = enemyEntity1->Geo->DrawArgs["cylinder"].StartIndexLocation;
	enemyEntity1->BaseVertexLocation = enemyEntity1->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	allEntities.push_back(std::move(enemyEntity1));
	enemyEntities.push_back(allEntities[currentEntityIndex].get());
	currentEntityIndex++;
	currentObjCBIndex++;

	auto enemyEntity2 = std::make_unique<Entity>();
	enemyEntity2->SetTranslation(22.0f, 1.0f, 4.0f);
	enemyEntity2->SetScale(1.0f, 2.0f, 1.0f);
	enemyEntity2->SetWorldMatrix();
	enemyEntity2->ObjCBIndex = currentObjCBIndex;
	enemyEntity2->Geo = Geometries["shapeGeo"].get();
	enemyEntity2->Mat = Materials["demo1"].get();
	enemyEntity2->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	enemyEntity2->IndexCount = enemyEntity2->Geo->DrawArgs["cylinder"].IndexCount;
	enemyEntity2->StartIndexLocation = enemyEntity2->Geo->DrawArgs["cylinder"].StartIndexLocation;
	enemyEntity2->BaseVertexLocation = enemyEntity2->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	allEntities.push_back(std::move(enemyEntity2));
	enemyEntities.push_back(allEntities[currentEntityIndex].get());
	currentEntityIndex++;
	currentObjCBIndex++;
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

		cmdList->DrawIndexedInstanced(e->IndexCount, 1, e->StartIndexLocation, e->BaseVertexLocation, 0);
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