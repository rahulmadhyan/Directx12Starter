#pragma once
#include <d3d12.h>
#include <vector>
#include "d3dx12.h"
#include "Vertex.h"

using namespace DirectX;

class Mesh
{
public:
	Mesh();
	Mesh(Vertex* vertices, int vertexCount, DWORD* indices, int indexCount, ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	Mesh(const char* fileName, ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	~Mesh();

	int GetIndexCount() const;
	D3D12_VERTEX_BUFFER_VIEW* GetVertexBuffer();
	D3D12_INDEX_BUFFER_VIEW* GetIndexBuffer();

private:

	int indexCount;

	ID3D12Resource* vertexBuffer; 
	ID3D12Resource* indexBuffer;

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView; 
	D3D12_INDEX_BUFFER_VIEW indexBufferView;

	void CreateBuffers(Vertex* vertices, int vertexCount, DWORD* indices, int indexCount, ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
};

