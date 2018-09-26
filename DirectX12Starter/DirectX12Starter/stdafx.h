#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN    // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"
#include "Vertex.h"
#include <string>

// will only call release if an object exists (prevents exceptions calling release on non existant objects)
#define SAFE_RELEASE(p) { if ( (p) ) { (p)->Release(); (p) = 0; } }

// this is the structure of our constant buffer.
struct ConstantBuffer {
	DirectX::XMFLOAT4 colorMultiplier;
};

HWND hwnd = NULL; // handle to the window
int Width = 800;
int Height = 600;
LPCTSTR WindowName = L"DX12Starter"; // name of the window 
LPCTSTR WindowTitle = L"DX12Starter"; // title of the window
bool FullScreen = false;

bool Running = true; // exit the program when this becomes false

const int frameBufferCount = 3; 
int frameIndex; // current RTV

int rtvDescriptorSize; // size of the RTV descriptor on the device (all front and back buffers will be the same size)
ID3D12Device* device; // Direct3D device

IDXGISwapChain3* swapChain; // swapchain used to switch between render targets

ID3D12CommandQueue* commandQueue; // container for command lists

ID3D12DescriptorHeap* rtvDescriptorHeap; // a descriptor heap to hold resources like the render targets

ID3D12Resource* renderTargets[frameBufferCount]; // number of render targets equal to buffer count

ID3D12CommandAllocator* commandAllocator[frameBufferCount]; // we want enough allocators for each buffer * number of threads (we only have one thread)

ID3D12GraphicsCommandList* commandList; // a command list we can record commands into, then execute them to render the frame

ID3D12Fence* fence[frameBufferCount];    // an object that is locked while our command list is being executed by the gpu. We need as many 
										 //as we have allocators (more if we want to know when the gpu is finished with an asset)

HANDLE fenceEvent; // handle to an event when our fence is unlocked by the gpu

UINT64 fenceValue[frameBufferCount]; // this value is incremented each frame. each fence will have its own value

ID3D12PipelineState* pipelineStateObject; // pso containing the pipeline state

ID3D12RootSignature* rootSignature; // defines data that the shader access

D3D12_VIEWPORT viewport;

D3D12_RECT scissorRect;

ID3D12Resource* vertexBuffer; // buffer in GPU memory

D3D12_VERTEX_BUFFER_VIEW vertexBufferView; // structure containing a pointer to the vertex data in gpu memory

ID3D12Resource* indexBuffer; 

D3D12_INDEX_BUFFER_VIEW indexBufferView;

ID3D12Resource* depthStencilBuffer;

ID3D12DescriptorHeap* dsDescriptorHeap;

ID3D12DescriptorHeap* mainDescriptorHeap[frameBufferCount]; // this heap will store the descriptor to our constant buffer

ID3D12Resource* constantBufferUploadHeap[frameBufferCount]; // memory on the GPU for the constant buffer

ConstantBuffer cbColorMultiplierData; // constant buffer data sent to the GPU

UINT8* cbColorMultiplierGPUAddress[frameBufferCount];

// create a window
bool InitializeWindow(HINSTANCE hInstance,
	int ShowWnd,
	bool fullscreen);

// main application loop
void mainloop();

// callback function for windows messages
LRESULT CALLBACK WndProc(HWND hWnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam);

bool InitD3D(); // initializes Direct3D 12

void Update(); 

void UpdatePipeline(); // update command lists

void Render(); // execute the command list

void Cleanup(); // release com ojects and clean up memory

void WaitForPreviousFrame(); // wait until gpu is finished with command list
