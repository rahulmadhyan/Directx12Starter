#include "Game.h"

const int gNumberFrameResources = 3;

Game::Game(HINSTANCE hInstance) : DXCore(hInstance)
{

}

Game::~Game()
{
	if (Device != nullptr)
		FlushCommandQueue();
}

bool Game::Initialize()
{
	if (!DXCore::Initialize())
		return false;

	// reset the command list to prep for initialization commands
	ThrowIfFailed(CommandList->Reset(CommandListAllocator.Get(), nullptr));

	mainCamera = Camera(screenWidth, screenHeight);

	systemData = new SystemData();

	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildGeometry();
	BuildRenderables();
	BuildFrameResources();
	BuildDescriptorHeaps();
	BuildConstantBufferViews();
	BuildPSOs();

	// execute the initialization commands
	ThrowIfFailed(CommandList->Close());
	ID3D12CommandList* cmdsLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
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
	mainCamera.Update();

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

	UpdateObjectCBs(timer);
	UpdateMainPassCB(timer);
}

void Game::Draw(const Timer &timer)
{
	auto cmdListAlloc = currentFrameResource->commandListAllocator;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(CommandList->Reset(CommandListAllocator.Get(), PSOs["opaque"].Get()));
	
	CommandList->RSSetViewports(1, &ScreenViewPort);
	CommandList->RSSetScissorRects(1, &ScissorRect);

	// Indicate a state transition on the resource usage.
	CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	CommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	CommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	CommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { CBVHeap.Get() };
	CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	CommandList->SetGraphicsRootSignature(rootSignature.Get());

	int passCbvIndex = PassCbvOffset + currentFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, CBVSRVUAVDescriptorSize);
	CommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

	DrawRenderables(CommandList.Get());

	// Indicate a state transition on the resource usage.
	CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(CommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(SwapChain->Present(0, 0));
	currentBackBuffer = (currentBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	currentFrameResource->Fence = ++currentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	CommandQueue->Signal(Fence.Get(), currentFence);
}

void Game::UpdateObjectCBs(const Timer & timer)
{
	auto currentObjectCB = currentFrameResource->ObjectCB.get();
	for (auto& e : renderables)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

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

	auto currPassCB = currentFrameResource->PassCB.get();
	currPassCB->CopyData(0, MainPassCB);
}

void Game::BuildDescriptorHeaps()
{
	UINT objCount = (UINT)renderables.size();

	// Need a CBV descriptor for each object for each frame resource,
	// +1 for the perPass CBV for each frame resource.
	UINT numDescriptors = (objCount + 1) * gNumberFrameResources;

	// Save an offset to the start of the pass CBVs.  These are the last 3 descriptors.
	PassCbvOffset = objCount * gNumberFrameResources;

	D3D12_DESCRIPTOR_HEAP_DESC CBVHeapDesc;
	CBVHeapDesc.NumDescriptors = numDescriptors;
	CBVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	CBVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	CBVHeapDesc.NodeMask = 0;
	ThrowIfFailed(Device->CreateDescriptorHeap(&CBVHeapDesc,
		IID_PPV_ARGS(&CBVHeap)));
}

void Game::BuildConstantBufferViews()
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	UINT objCount = (UINT)renderables.size();

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
}

void Game::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE cbvTable0;
	cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE cbvTable1;
	cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	// Create root CBVs.
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
	slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr,
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
	Shaders["VS"] = d3dUtil::CompileShader(L"VertexShader.hlsl", nullptr, "main", "vs_5_1");
	Shaders["PS"] = d3dUtil::CompileShader(L"PixelShader.hlsl", nullptr, "main", "ps_5_1");

	inputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void Game::BuildGeometry()
{
	//GeometryGenerator geoGen;
	//GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
	//GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	//GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	//GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

	////
	//// We are concatenating all the geometry into one big vertex/index buffer.  So
	//// define the regions in the buffer each submesh covers.
	////

	//// Cache the vertex offsets to each object in the concatenated vertex buffer.
	//UINT boxVertexOffset = 0;
	//UINT gridVertexOffset = (UINT)box.Vertices.size();
	//UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	//UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();
	//UINT box1VertexOffset = cylinderVertexOffset + systemData->subSystemData["box1"].count;

	//// Cache the starting index for each object in the concatenated index buffer.
	//UINT boxIndexOffset = 0;
	//UINT gridIndexOffset = (UINT)box.Indices32.size();
	//UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	//UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();
	//UINT box1IndexOffset = cylinderIndexOffset + systemData->subSystemData["box1"].count;

	//// Define the SubmeshGeometry that cover different 
	//// regions of the vertex/index buffers.

	//SubmeshGeometry boxSubmesh;
	//boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	//boxSubmesh.StartIndexLocation = boxIndexOffset;
	//boxSubmesh.BaseVertexLocation = boxVertexOffset;

	//SubmeshGeometry gridSubmesh;
	//gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	//gridSubmesh.StartIndexLocation = gridIndexOffset;
	//gridSubmesh.BaseVertexLocation = gridVertexOffset;

	//SubmeshGeometry sphereSubmesh;
	//sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	//sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	//sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	//SubmeshGeometry cylinderSubmesh;
	//cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	//cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	//cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	//SubmeshGeometry box1Submesh;
	//box1Submesh.IndexCount = (UINT)systemData->subSystemData["box1"].count;
	//box1Submesh.StartIndexLocation = box1IndexOffset;
	//box1Submesh.BaseVertexLocation = box1VertexOffset;

	////
	//// Extract the vertex elements we are interested in and pack the
	//// vertices of all the meshes into one vertex buffer.
	////

	//auto totalVertexCount =
	//	box.Vertices.size() +
	//	grid.Vertices.size() +
	//	sphere.Vertices.size() +
	//	cylinder.Vertices.size() +
	//	systemData->subSystemData["box1"].count;

	//std::vector<Vertex> vertices(totalVertexCount);

	//UINT k = 0;
	//for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	//{
	//	vertices[k].Position = box.Vertices[i].Position;
	//	vertices[k].Color = XMFLOAT4(DirectX::Colors::DarkGreen);
	//}

	//for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	//{
	//	vertices[k].Position = grid.Vertices[i].Position;
	//	vertices[k].Color = XMFLOAT4(DirectX::Colors::ForestGreen);
	//}

	//for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	//{
	//	vertices[k].Position = sphere.Vertices[i].Position;
	//	vertices[k].Color = XMFLOAT4(DirectX::Colors::Crimson);
	//}

	//for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	//{
	//	vertices[k].Position = cylinder.Vertices[i].Position;
	//	vertices[k].Color = XMFLOAT4(DirectX::Colors::SteelBlue);
	//}

	//for (size_t i = 0; i < systemData->subSystemData["box1"].count; ++i, ++k)
	//{
	//	vertices[k].Position = systemData->positions[systemData->subSystemData["box1"].baseLocation + i];
	//	vertices[k].Color = systemData->colors[systemData->subSystemData["box1"].baseLocation + i];
	//}

	//std::vector<std::uint16_t> indices;
	//indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	//indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	//indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	//indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

	systemData->LoadOBJFile("Resources/Models/cube.obj", Device, "box1");
	SubmeshGeometry box1Submesh;
	box1Submesh.IndexCount = (UINT)systemData->subSystemData["box1"].count;
	box1Submesh.StartIndexLocation = systemData->subSystemData["box1"].baseLocation;
	box1Submesh.BaseVertexLocation = systemData->subSystemData["box1"].baseLocation;

	systemData->LoadOBJFile("Resources/Models/cylinder.obj", Device, "cylinder");
	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)systemData->subSystemData["cylinder"].count;
	cylinderSubmesh.StartIndexLocation = systemData->subSystemData["cylinder"].baseLocation;
	cylinderSubmesh.BaseVertexLocation = systemData->subSystemData["cylinder"].baseLocation;

	std::vector<Vertex> vertices(systemData->currentBaseLocation);
	std::vector<std::uint16_t> indices(systemData->currentBaseLocation);
	UINT k = 0;

	for (size_t i = 0; i < systemData->currentBaseLocation; ++i, ++k)
	{
		vertices[k].Position = systemData->positions[i];
		vertices[k].Color = XMFLOAT4(DirectX::Colors::ForestGreen);
		indices[k] = systemData->indices[i];
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
			1, (UINT)renderables.size()));
	}
}

void Game::BuildRenderables()
{
	auto boxRitem = new Renderable;
	XMStoreFloat4x4(&boxRitem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f)*XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	boxRitem->ObjCBIndex = 0;
	boxRitem->Geo = Geometries["shapeGeo"].get();
	boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box1"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box1"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box1"].BaseVertexLocation;
	renderables.push_back(std::move(boxRitem));

	auto cylinderRitem = new Renderable;
	XMStoreFloat4x4(&cylinderRitem->World, XMMatrixScaling(2.0f, 4.0f, 2.0f)*XMMatrixTranslation(0.0f, 0.5f, 0.0f));	
	cylinderRitem->ObjCBIndex = 1;
	cylinderRitem->Geo = Geometries["shapeGeo"].get();
	cylinderRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	cylinderRitem->IndexCount = cylinderRitem->Geo->DrawArgs["cylinder"].IndexCount;
	cylinderRitem->StartIndexLocation = cylinderRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
	cylinderRitem->BaseVertexLocation = cylinderRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	renderables.push_back(std::move(cylinderRitem));

	//UINT objCBIndex = 2;
	//for (int i = 0; i < 5; ++i)
	//{
	//	auto leftCylRitem = new Renderable;
	//	auto rightCylRitem = new Renderable;
	//	auto leftSphereRitem = new Renderable;
	//	auto rightSphereRitem = new Renderable;

	//	XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
	//	XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);

	//	XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
	//	XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);

	//	XMStoreFloat4x4(&leftCylRitem->World, rightCylWorld);
	//	leftCylRitem->ObjCBIndex = objCBIndex++;
	//	leftCylRitem->Geo = Geometries["shapeGeo"].get();
	//	leftCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//	leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
	//	leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
	//	leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

	//	XMStoreFloat4x4(&rightCylRitem->World, leftCylWorld);
	//	rightCylRitem->ObjCBIndex = objCBIndex++;
	//	rightCylRitem->Geo = Geometries["shapeGeo"].get();
	//	rightCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//	rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
	//	rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
	//	rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

	//	XMStoreFloat4x4(&leftSphereRitem->World, leftSphereWorld);
	//	leftSphereRitem->ObjCBIndex = objCBIndex++;
	//	leftSphereRitem->Geo = Geometries["shapeGeo"].get();
	//	leftSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//	leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
	//	leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	//	leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

	//	XMStoreFloat4x4(&rightSphereRitem->World, rightSphereWorld);
	//	rightSphereRitem->ObjCBIndex = objCBIndex++;
	//	rightSphereRitem->Geo = Geometries["shapeGeo"].get();
	//	rightSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//	rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
	//	rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	//	rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

	//	renderables.push_back(std::move(leftCylRitem));
	//	renderables.push_back(std::move(rightCylRitem));
	//	renderables.push_back(std::move(leftSphereRitem));
	//	renderables.push_back(std::move(rightSphereRitem));
	//}
}

void Game::DrawRenderables(ID3D12GraphicsCommandList* cmdList)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objectCB = currentFrameResource->ObjectCB->Resource();

	// For each render item...
	for (size_t i = 0; i < renderables.size(); ++i)
	{
		auto r = renderables[i];

		cmdList->IASetVertexBuffers(0, 1, &r->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&r->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(r->PrimitiveType);

		// Offset to the CBV in the descriptor heap for this object and for this frame resource.
		UINT cbvIndex = currentFrameResourceIndex * (UINT)renderables.size() + r->ObjCBIndex;
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, CBVSRVUAVDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);

		cmdList->DrawIndexedInstanced(r->IndexCount, 1, r->StartIndexLocation, r->BaseVertexLocation, 0);
	}
}
