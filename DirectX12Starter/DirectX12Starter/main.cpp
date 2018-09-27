#include "stdafx.h"

int WINAPI WinMain(HINSTANCE hInstance,    //Main windows function
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nShowCmd)
{
	// create the window
	if (!InitializeWindow(hInstance, nShowCmd, FullScreen))
	{
		MessageBox(0, L"Window Initialization - Failed",
			L"Error", MB_OK);
		Cleanup();
		return 1;
	}

	// initialize direct3d
	if (!InitD3D())
	{
		MessageBox(0, L"Failed to initialize Direct3D 12",
			L"Error", MB_OK);
		Cleanup();
		return 1;
	}

	// start the main loop
	mainloop();

	// we want to wait for the gpu to finish executing the command list before we start releasing everything
	WaitForPreviousFrame();

	// close the fence event
	CloseHandle(fenceEvent);

	Cleanup();

	return 0;
}

bool InitializeWindow(HINSTANCE hInstance,
	int ShowWnd,
	bool fullscreen)
{
	if (fullscreen)
	{
		HMONITOR hmon = MonitorFromWindow(hwnd,
			MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi = { sizeof(mi) };
		GetMonitorInfo(hmon, &mi);

		Width = mi.rcMonitor.right - mi.rcMonitor.left;
		Height = mi.rcMonitor.bottom - mi.rcMonitor.top;
	}

	WNDCLASSEX wc;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = NULL;
	wc.cbWndExtra = NULL;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = WindowName;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wc))
	{
		MessageBox(NULL, L"Error registering class",
			L"Error", MB_OK | MB_ICONERROR);
		return false;
	}

	hwnd = CreateWindowEx(NULL,
		WindowName,
		WindowTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		Width, Height,
		NULL,
		NULL,
		hInstance,
		NULL);

	if (!hwnd)
	{
		MessageBox(NULL, L"Error creating window",
			L"Error", MB_OK | MB_ICONERROR);
		return false;
	}

	if (fullscreen)
	{
		SetWindowLong(hwnd, GWL_STYLE, 0);
	}

	ShowWindow(hwnd, ShowWnd);
	UpdateWindow(hwnd);

	return true;
}

void mainloop() {
	MSG msg;
	ZeroMemory(&msg, sizeof(MSG));

	while (true)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				break;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			// run game code
			Update();
			Render();
		}
	}
}

LRESULT CALLBACK WndProc(HWND hwnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam)
{
	switch (msg)
	{

	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) {
			if (MessageBox(0, L"Are you sure you want to exit?",
				L"Really?", MB_YESNO | MB_ICONQUESTION) == IDYES)
			{
				Running = false;
				DestroyWindow(hwnd);
			}
		}
		return 0;

	case WM_DESTROY:
		Running = false;
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd,
		msg,
		wParam,
		lParam);
}

bool InitD3D()
{
	HRESULT hr;

	IDXGIFactory4* dxgiFactory;
	hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	if (FAILED(hr))
	{
		return false;
	}

	IDXGIAdapter1* adapter; // adapters are the graphics card
	int adapterIndex = 0;
	bool adapterFound = false;

	while (dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// dont want a software device
			continue;
		}

		hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr);
		if (SUCCEEDED(hr))
		{
			adapterFound = true;
			break;
		}

		adapterIndex++;
	}

	if (!adapterFound)
	{
		return false;
	}

	// create the device
	hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));

	if (FAILED(hr))
	{
		return false;
	}
	
	D3D12_COMMAND_QUEUE_DESC cqDesc = {}; // using all default values
	cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // direct means the gpu can directly execute this command queue

	hr = device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&commandQueue));
	if (FAILED(hr))
	{
		return false;
	}

	DXGI_MODE_DESC backBufferDesc = {}; // describe the dislay mode
	backBufferDesc.Width = Width;
	backBufferDesc.Height = Height;
	backBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	// describe multi-sampling. Not multisampling (set count to 1)
	DXGI_SAMPLE_DESC sampleDesc = {};
	sampleDesc.Count = 1;

	//describe swap chain
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = frameBufferCount;
	swapChainDesc.BufferDesc = backBufferDesc;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // this says the pipeline will render to this swap chain
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // dxgi will discard the buffer (data) after we call present
	swapChainDesc.OutputWindow = hwnd;
	swapChainDesc.SampleDesc = sampleDesc;
	swapChainDesc.Windowed = !FullScreen;

	IDXGISwapChain* tempSwapChain;

	dxgiFactory->CreateSwapChain(
		commandQueue, // the queue will be flushed once the swap chain is created
		&swapChainDesc, // give it the swap chain description we created above
		&tempSwapChain // store the created swap chain in a temp IDXGISwapChain interface
	);

	swapChain = static_cast<IDXGISwapChain3*>(tempSwapChain);

	frameIndex = swapChain->GetCurrentBackBufferIndex();

	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = frameBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // heap is a render target view heap

	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // not referenced directly by shaders
	hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
	if (FAILED(hr))
	{
		return false;
	}

	// get the size of a descriptor in this heap (this is a rtv heap, so only rtv descriptors should be stored in it.
	// descriptor sizes may vary from device to device, which is why there is no set size and we must ask the 
	// device to give us the size. we will use this size to increment a descriptor handle offset
	rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// get a handle to the first descriptor in the descriptor heap. a handle is basically a pointer,
	// but we cannot literally use it like a c++ pointer.
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	// create a RTV for each frame buffer
	for (int i = 0; i < frameBufferCount; i++)
	{
		// first we get the n'th buffer in the swap chain and store it in the n'th
		// position of our ID3D12Resource array
		hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
		if (FAILED(hr))
		{
			return false;
		}

		// the we "create" a render target view which binds the swap chain buffer (ID3D12Resource[n]) to the rtv handle
		device->CreateRenderTargetView(renderTargets[i], nullptr, rtvHandle);

		// we increment the rtv handle by the rtv descriptor size we got above
		rtvHandle.Offset(1, rtvDescriptorSize);
	}

	for (int i = 0; i < frameBufferCount; i++)
	{
		hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator[i]));
		if (FAILED(hr))
		{
			return false;
		}
	}

	// create the command list with the first allocator
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator[frameIndex], NULL, IID_PPV_ARGS(&commandList));
	if (FAILED(hr))
	{
		return false;
	}


	// create the fences
	for (int i = 0; i < frameBufferCount; i++)
	{
		hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence[i]));
		if (FAILED(hr))
		{
			return false;
		}
		fenceValue[i] = 0; // set the initial fence value to 0
	}

	// create a handle to a fence event
	fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fenceEvent == nullptr)
	{
		return false;
	}

	// create a root descriptor
	D3D12_ROOT_DESCRIPTOR rootCBVDescriptor;
	rootCBVDescriptor.RegisterSpace = 0;
	rootCBVDescriptor.ShaderRegister = 0;
																								 
	D3D12_ROOT_PARAMETER  rootParameters[1]; 
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor = rootCBVDescriptor; 
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // pixel shader will be the only shader accessing this parameter

	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(_countof(rootParameters), 
		rootParameters, 
		0,
		nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | // deny shader stages here for better performance
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS);

	ID3DBlob* signature;
	hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr);
	if (FAILED(hr))
	{
		return false;
	}

	hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	if (FAILED(hr))
	{
		return false;
	}

	ID3DBlob* vertexShader;
	ID3DBlob* errorBuffer;

	hr = D3DCompileFromFile(L"VertexShader.hlsl",
		nullptr,
		nullptr,
		"main",
		"vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&vertexShader,
		&errorBuffer);

	if (FAILED(hr))
	{
		OutputDebugStringA((char*)errorBuffer->GetBufferPointer());
		return false;
	}

	D3D12_SHADER_BYTECODE vertexShaderBytecode = {};
	vertexShaderBytecode.BytecodeLength = vertexShader->GetBufferSize();
	vertexShaderBytecode.pShaderBytecode = vertexShader->GetBufferPointer();

	ID3DBlob* pixelShader;
	hr = D3DCompileFromFile(L"PixelShader.hlsl",
		nullptr,
		nullptr,
		"main",
		"ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&pixelShader,
		&errorBuffer);
	if (FAILED(hr))
	{
		OutputDebugStringA((char*)errorBuffer->GetBufferPointer());
		return false;
	}

	D3D12_SHADER_BYTECODE pixelShaderBytecode = {};
	pixelShaderBytecode.BytecodeLength = pixelShader->GetBufferSize();
	pixelShaderBytecode.pShaderBytecode = pixelShader->GetBufferPointer();

	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};

	inputLayoutDesc.NumElements = sizeof(inputLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC);
	inputLayoutDesc.pInputElementDescs = inputLayout;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {}; // structure to define a pso
	psoDesc.InputLayout = inputLayoutDesc; // structure describing our input layout
	psoDesc.pRootSignature = rootSignature; // root signature that describes the input data this pso needs
	psoDesc.VS = vertexShaderBytecode; 
	psoDesc.PS = pixelShaderBytecode; 
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; 
	psoDesc.SampleDesc = sampleDesc; 
	psoDesc.SampleMask = 0xffffffff; // sample mask has to do with multi-sampling. 0xffffffff means point sampling is done
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); 
	psoDesc.NumRenderTargets = 1; // only binding one render target
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // default depth stencil state

	// create the PSO
	hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStateObject));
	if (FAILED(hr))
	{
		return false;
	}

	Vertex vertices[] = {
		// front face
		{ -0.5f,  0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
		{  0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f },
		{ -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
		{  0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f },

		// right side face
		{  0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
		{  0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 1.0f },
		{  0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
		{  0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f },

		// left side face
		{ -0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
		{ -0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f },
		{ -0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
		{ -0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f },

		// back face
		{  0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
		{ -0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 1.0f },
		{  0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
		{ -0.5f,  0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f },

		// top face
		{ -0.5f,  0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
		{ 0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 1.0f },
		{ 0.5f,  0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
		{ -0.5f,  0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f },

		// bottom face
		{  0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
		{ -0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f },
		{  0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
		{ -0.5f, -0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
	};

	int vBufferSize = sizeof(vertices);

	// default heap is memory on the GPU. Only the GPU has access to this memory
	// To get data into this heap, we will have to upload the data using
	// an upload heap
	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(vBufferSize), // resource description for a buffer
		D3D12_RESOURCE_STATE_COPY_DEST, // we will start this heap in the copy destination state since we will copy data
										// from the upload heap to this heap
		nullptr, // optimized clear value must be null for this type of resource. used for render targets and depth/stencil buffers
		IID_PPV_ARGS(&vertexBuffer));

	// we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
	vertexBuffer->SetName(L"Vertex Buffer Resource Heap");

	// upload heaps are used to upload data to the GPU. CPU can write to it, GPU can read from it
	// We will upload the vertex buffer using this heap to the default heap
	ID3D12Resource* vBufferUploadHeap;
	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), 
		D3D12_HEAP_FLAG_NONE, 
		&CD3DX12_RESOURCE_DESC::Buffer(vBufferSize), // resource description for a buffer
		D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
		nullptr,
		IID_PPV_ARGS(&vBufferUploadHeap));
	vBufferUploadHeap->SetName(L"Vertex Buffer Upload Resource Heap");

	// store vertex buffer in upload heap
	D3D12_SUBRESOURCE_DATA vertexData = {};
	vertexData.pData = reinterpret_cast<BYTE*>(vertices); // pointer to our vertex array
	vertexData.RowPitch = vBufferSize; // size of all our triangle vertex data
	vertexData.SlicePitch = vBufferSize; // also the size of our triangle vertex data

	// creating a command with the command list to copy the data from
	// the upload heap to the default heap
	UpdateSubresources(commandList, vertexBuffer, vBufferUploadHeap, 0, 0, 1, &vertexData);

	// transition the vertex buffer data from copy destination state to vertex buffer state
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	DWORD indices[] = {
		// ffront face
		0, 1, 2, // first triangle
		0, 3, 1, // second triangle

		// left face
		4, 5, 6, // first triangle
		4, 7, 5, // second triangle

		// right face
		8, 9, 10, // first triangle
		8, 11, 9, // second triangle

		// back face
		12, 13, 14, // first triangle
		12, 15, 13, // second triangle

		// top face
		16, 17, 18, // first triangle
		16, 19, 17, // second triangle

		// bottom face
		20, 21, 22, // first triangle
		20, 23, 21, // second triangle
	};

	int indexBufferSize = sizeof(indices);
	numCubeIndices = sizeof(indices) / sizeof(DWORD);

	// create default heap to hold index buffer
	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&indexBuffer));

	indexBuffer->SetName(L"Index Buffer Resource Heap");

	// create upload heap to upload index buffer
	ID3D12Resource* indexBufferUploadHeap;
	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
		D3D12_HEAP_FLAG_NONE, // no flags
		&CD3DX12_RESOURCE_DESC::Buffer(vBufferSize), // resource description for a buffer
		D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
		nullptr,
		IID_PPV_ARGS(&indexBufferUploadHeap));
	indexBufferUploadHeap->SetName(L"Index Buffer Upload Resource Heap");

	D3D12_SUBRESOURCE_DATA indexData = {};
	indexData.pData = reinterpret_cast<BYTE*>(indices);
	indexData.RowPitch = indexBufferSize;
	indexData.SlicePitch = indexBufferSize;

	// create command with commandlist to copy data from upload heap to default heap
	UpdateSubresources(commandList, indexBuffer, indexBufferUploadHeap, 0, 0, 1, &indexData);

	// transition the index buffer data from copy destination state to index buffer state
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	// create a index buffer view for the triangle. We get the GPU memory address to the index pointer using the GetGPUVirtualAddress() method
	indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	indexBufferView.Format = DXGI_FORMAT_R32_UINT; // 32-bit unsigned integer (this is what a dword is, double word, a word is 2 bytes)
	indexBufferView.SizeInBytes = indexBufferSize;

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsDescriptorHeap));
	if (FAILED(hr))
	{
		return false;
	}

	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
	depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
	depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
	depthOptimizedClearValue.DepthStencil.Stencil = 0;

	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, Width, Height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthOptimizedClearValue,
		IID_PPV_ARGS(&depthStencilBuffer)
	);

	hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsDescriptorHeap));
	if (FAILED(hr))
	{
		Running = false;
	}

	dsDescriptorHeap->SetName(L"Depth / Stencil Resource Heap");

	device->CreateDepthStencilView(depthStencilBuffer, &depthStencilDesc, dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	// create the constant buffer resource heap
	// We will update the constant buffer one or more times per frame, so we will use only an upload heap
	// unlike previously we used an upload heap to upload the vertex and index data, and then copied over
	// to a default heap. If you plan to use a resource for more than a couple frames, it is usually more
	// efficient to copy to a default heap where it stays on the gpu. In this case, our constant buffer
	// will be modified and uploaded at least once per frame, so we only use an upload heap

	// create a resource heap, descriptor heap, and pointer to cbv for each frame
	for (int i = 0; i < frameBufferCount; ++i)
	{
		hr = device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // this heap will be used to upload the constant buffer data
			D3D12_HEAP_FLAG_NONE, // no flags
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64), // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
			D3D12_RESOURCE_STATE_GENERIC_READ, // will be data that is read from so we keep it in the generic read state
			nullptr, // we do not have use an optimized clear value for constant buffers
			IID_PPV_ARGS(&constantBufferUploadHeaps[i]));
		constantBufferUploadHeaps[i]->SetName(L"Constant Buffer Upload Resource Heap");

		ZeroMemory(&cbPerObject, sizeof(cbPerObject));

		CD3DX12_RANGE readRange(0, 0);    // We do not intend to read from this resource on the CPU. (so end is less than or equal to begin)

		// map the resource heap to get a gpu virtual address to the beginning of the heap
		hr = constantBufferUploadHeaps[i]->Map(0, &readRange, reinterpret_cast<void**>(&cbvGPUAddress[i]));

		// Because of the constant read alignment requirements, constant buffer views must be 256 bit aligned. Our buffers are smaller than 256 bits,
		// so we need to add spacing between the two buffers, so that the second buffer starts at 256 bits from the beginning of the resource heap.
		memcpy(cbvGPUAddress[i], &cbPerObject, sizeof(cbPerObject)); // cube1's constant buffer data
		memcpy(cbvGPUAddress[i] + ConstantBufferPerObjectAlignedSize, &cbPerObject, sizeof(cbPerObject)); // cube2's constant buffer data
	}

	// execute the command list to upload the initial assets (triangle data)
	commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// increment the fence value now, otherwise the buffer might not be uploaded by the time we start drawing
	fenceValue[frameIndex]++;
	hr = commandQueue->Signal(fence[frameIndex], fenceValue[frameIndex]);
	if (FAILED(hr))
	{
		Running = false;
	}

	// create a vertex buffer view for the triangle. We get the GPU memory address to the vertex pointer using the GetGPUVirtualAddress() method
	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.StrideInBytes = sizeof(Vertex);
	vertexBufferView.SizeInBytes = vBufferSize;

	// fill out the Viewport
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = Width;
	viewport.Height = Height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	// Fill out a scissor rect
	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = Width;
	scissorRect.bottom = Height;

	// build projection and view matrix
	XMMATRIX tmpMat = XMMatrixPerspectiveFovLH(45.0f*(3.14f / 180.0f), (float)Width / (float)Height, 0.1f, 1000.0f);
	XMStoreFloat4x4(&cameraProjMat, tmpMat);

	// set starting camera state
	cameraPosition = XMFLOAT4(0.0f, 2.0f, -4.0f, 0.0f);
	cameraTarget = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	cameraUp = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);

	// build view matrix
	XMVECTOR cPos = XMLoadFloat4(&cameraPosition);
	XMVECTOR cTarg = XMLoadFloat4(&cameraTarget);
	XMVECTOR cUp = XMLoadFloat4(&cameraUp);
	tmpMat = XMMatrixLookAtLH(cPos, cTarg, cUp);
	XMStoreFloat4x4(&cameraViewMat, tmpMat);

	// set starting cubes position
	// first cube
	cube1Position = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f); // set cube 1's position
	XMVECTOR posVec = XMLoadFloat4(&cube1Position); // create xmvector for cube1's position

	tmpMat = XMMatrixTranslationFromVector(posVec); // create translation matrix from cube1's position vector
	XMStoreFloat4x4(&cube1RotMat, XMMatrixIdentity()); // initialize cube1's rotation matrix to identity matrix
	XMStoreFloat4x4(&cube1WorldMat, tmpMat); // store cube1's world matrix

	// second cube
	cube2PositionOffset = XMFLOAT4(1.5f, 0.0f, 0.0f, 0.0f);
	posVec = XMLoadFloat4(&cube2PositionOffset) + XMLoadFloat4(&cube1Position); // create xmvector for cube2's position
																				// we are rotating around cube1 here, so add cube2's position to cube1

	tmpMat = XMMatrixTranslationFromVector(posVec); // create translation matrix from cube2's position offset vector
	XMStoreFloat4x4(&cube2RotMat, XMMatrixIdentity()); // initialize cube2's rotation matrix to identity matrix
	XMStoreFloat4x4(&cube2WorldMat, tmpMat); // store cube2's world matrix

	return true;
}

void Update()
{
	// create rotation matrices
	XMMATRIX rotXMat = XMMatrixRotationX(0.0001f);
	XMMATRIX rotYMat = XMMatrixRotationY(0.0002f);
	XMMATRIX rotZMat = XMMatrixRotationZ(0.0003f);

	// add rotation to cube1's rotation matrix and store it
	XMMATRIX rotMat = XMLoadFloat4x4(&cube1RotMat) * rotXMat * rotYMat * rotZMat;
	XMStoreFloat4x4(&cube1RotMat, rotMat);

	// create translation matrix for cube 1 from cube 1's position vector
	XMMATRIX translationMat = XMMatrixTranslationFromVector(XMLoadFloat4(&cube1Position));

	// create cube1's world matrix by first rotating the cube, then positioning the rotated cube
	XMMATRIX worldMat = rotMat * translationMat;

	// store cube1's world matrix
	XMStoreFloat4x4(&cube1WorldMat, worldMat);

	// update constant buffer for cube1
	// create the wvp matrix and store in constant buffer
	XMMATRIX viewMat = XMLoadFloat4x4(&cameraViewMat); // load view matrix
	XMMATRIX projMat = XMLoadFloat4x4(&cameraProjMat); // load projection matrix
	XMMATRIX wvpMat = XMLoadFloat4x4(&cube1WorldMat) * viewMat * projMat; // create wvp matrix
	XMMATRIX transposed = XMMatrixTranspose(wvpMat); // must transpose wvp matrix for the gpu
	XMStoreFloat4x4(&cbPerObject.wvpMat, transposed); // store transposed wvp matrix in constant buffer

	// copy our ConstantBuffer instance to the mapped constant buffer resource
	memcpy(cbvGPUAddress[frameIndex], &cbPerObject, sizeof(cbPerObject));

	// now do cube2's world matrix
	// create rotation matrices for cube2
	rotXMat = XMMatrixRotationX(0.0003f);
	rotYMat = XMMatrixRotationY(0.0002f);
	rotZMat = XMMatrixRotationZ(0.0001f);

	// add rotation to cube2's rotation matrix and store it
	rotMat = rotZMat * (XMLoadFloat4x4(&cube2RotMat) * (rotXMat * rotYMat));
	XMStoreFloat4x4(&cube2RotMat, rotMat);

	// create translation matrix for cube 2 to offset it from cube 1 (its position relative to cube1
	XMMATRIX translationOffsetMat = XMMatrixTranslationFromVector(XMLoadFloat4(&cube2PositionOffset));

	// we want cube 2 to be half the size of cube 1, so we scale it by .5 in all dimensions
	XMMATRIX scaleMat = XMMatrixScaling(0.5f, 0.5f, 0.5f);

	// reuse worldMat. 
	// first we scale cube2. scaling happens relative to point 0,0,0, so you will almost always want to scale first
	// then we translate it. 
	// then we rotate it. rotation always rotates around point 0,0,0
	// finally we move it to cube 1's position, which will cause it to rotate around cube 1
	worldMat = scaleMat * translationOffsetMat * rotMat * translationMat;

	wvpMat = XMLoadFloat4x4(&cube2WorldMat) * viewMat * projMat; // create wvp matrix
	transposed = XMMatrixTranspose(wvpMat); // must transpose wvp matrix for the gpu
	XMStoreFloat4x4(&cbPerObject.wvpMat, transposed); // store transposed wvp matrix in constant buffer

	// copy our ConstantBuffer instance to the mapped constant buffer resource
	memcpy(cbvGPUAddress[frameIndex] + ConstantBufferPerObjectAlignedSize, &cbPerObject, sizeof(cbPerObject));

	// store cube2's world matrix
	XMStoreFloat4x4(&cube2WorldMat, worldMat);
}

void UpdatePipeline()
{
	HRESULT hr;
	
	// We have to wait for the gpu to finish with the command allocator before we reset it
	WaitForPreviousFrame();

	// we can only reset an allocator once the gpu is done with it
	// resetting an allocator frees the memory that the command list was stored in
	hr = commandAllocator[frameIndex]->Reset();
	if (FAILED(hr))
	{
		Running = false;
	}

	// reset the command list. by resetting the command list we are putting it into
	// a recording state so we can start recording commands into the command allocator.
	// the command allocator that we reference here may have multiple command lists
	// associated with it, but only one can be recording at any time. Make sure
	// that any other command lists associated to this command allocator are in
	// the closed state (not recording).
	// Here you will pass an initial pipeline state object as the second parameter,
	// but in this tutorial we are only clearing the rtv, and do not actually need
	// anything but an initial default pipeline, which is what we get by setting
	// the second parameter to NULL
	hr = commandList->Reset(commandAllocator[frameIndex], pipelineStateObject);
	if (FAILED(hr))
	{
		Running = false;
	}

	// transition the "frameIndex" render target from the present state to the render target state so the command list draws to it starting from here
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// here we again get the handle to our current render target view so we can set it as the render target in the output merger stage of the pipeline
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);

	// get a handle to the depth/stencil buffer
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	// set the render target for the output merger stage (the output of the pipeline)
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// Clear the render target by using the ClearRenderTargetView command
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	// clear the depth/stencil buffer
	commandList->ClearDepthStencilView(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// set root signature
	commandList->SetGraphicsRootSignature(rootSignature); // set the root signature

	commandList->RSSetViewports(1, &viewport); 
	commandList->RSSetScissorRects(1, &scissorRect); 
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView); 
	commandList->IASetIndexBuffer(&indexBufferView);
	
	// set cube1's constant buffer
	commandList->SetGraphicsRootConstantBufferView(0, constantBufferUploadHeaps[frameIndex]->GetGPUVirtualAddress());

	// draw first cube
	commandList->DrawIndexedInstanced(numCubeIndices, 1, 0, 0, 0);

	// second cube

	// set cube2's constant buffer. You can see we are adding the size of ConstantBufferPerObject to the constant buffer
	// resource heaps address. This is because cube1's constant buffer is stored at the beginning of the resource heap, while
	// cube2's constant buffer data is stored after (256 bits from the start of the heap).
	commandList->SetGraphicsRootConstantBufferView(0, constantBufferUploadHeaps[frameIndex]->GetGPUVirtualAddress() + ConstantBufferPerObjectAlignedSize);

	// draw second cube
	commandList->DrawIndexedInstanced(numCubeIndices, 1, 0, 0, 0);


	// transition the "frameIndex" render target from the render target state to the present state. If the debug layer is enabled, you will receive a
	// warning if present is called on the render target when it's not in the present state
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	hr = commandList->Close();
	if (FAILED(hr))
	{
		Running = false;
	}
}

void Render()
{
	HRESULT hr;

	UpdatePipeline();

	ID3D12CommandList* ppCommanLists[] = { commandList }; // array of command lists

	commandQueue->ExecuteCommandLists(_countof(ppCommanLists), ppCommanLists); // execute the array of command lists

	// this command goes in at the end of our command queue. we will know when our command queue 
	// has finished because the fence value will be set to "fenceValue" from the GPU since the command
	// queue is being executed on the GPU
	hr = commandQueue->Signal(fence[frameIndex], fenceValue[frameIndex]); 
	if (FAILED(hr))
	{
		Running = false;
	}

	hr = swapChain->Present(0, 0); // present the current backbuffer
	if (FAILED(hr))
	{
		Running = false;
	}
}

void Cleanup()
{
	// wait for the gpu to finish all frames
	for (int i = 0; i < frameBufferCount; ++i)
	{
		frameIndex = i;
		WaitForPreviousFrame();
	}

	// get swapchain out of full screen before exiting
	BOOL fs = false;
	if (swapChain->GetFullscreenState(&fs, NULL))
		swapChain->SetFullscreenState(false, NULL);

	SAFE_RELEASE(device);
	SAFE_RELEASE(swapChain);
	SAFE_RELEASE(commandQueue);
	SAFE_RELEASE(rtvDescriptorHeap);
	SAFE_RELEASE(commandList);

	for (int i = 0; i < frameBufferCount; ++i)
	{
		SAFE_RELEASE(renderTargets[i]);
		SAFE_RELEASE(commandAllocator[i]);
		SAFE_RELEASE(fence[i]);
	};

	SAFE_RELEASE(pipelineStateObject);
	SAFE_RELEASE(rootSignature);
	SAFE_RELEASE(vertexBuffer);
	SAFE_RELEASE(indexBuffer);

	SAFE_RELEASE(depthStencilBuffer);
	SAFE_RELEASE(dsDescriptorHeap);

	for (int i = 0; i < frameBufferCount; ++i)
	{
		SAFE_RELEASE(constantBufferUploadHeaps[i]);
	};
}

void WaitForPreviousFrame()
{
	HRESULT hr;
	
	// swap the current rtv buffer index so we draw on the correct buffer
	frameIndex = swapChain->GetCurrentBackBufferIndex();

	// if the current fence value is still less than "fenceValue", then we know the GPU has not finished executing
	// the command queue since it has not reached the "commandQueue->Signal(fence, fenceValue)" command
	if (fence[frameIndex]->GetCompletedValue() < fenceValue[frameIndex])
	{
		// we have the fence create an event which is signaled once the fence's current value is "fenceValue"
		hr = fence[frameIndex]->SetEventOnCompletion(fenceValue[frameIndex], fenceEvent);
		if (FAILED(hr))
		{
			Running = false;
		}

		// We will wait until the fence has triggered the event that it's current value has reached "fenceValue". once it's value
		// has reached "fenceValue", we know the command queue has finished executing
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	// increment fenceValue for next frame
	fenceValue[frameIndex]++;
}