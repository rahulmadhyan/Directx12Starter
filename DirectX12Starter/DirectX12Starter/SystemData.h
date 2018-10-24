#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <DirectXMath.h>
#include <d3d12.h>
#include "Vertex.h"
#include "wrl.h"

using namespace DirectX;

#define DATA_SIZE 65535

struct SubSystem
{
	uint16_t baseLocation;
	uint16_t count;

	SubSystem()
	{
		baseLocation = 0;
		count = 0;
	}
};

class SystemData
{
public:
	SystemData();
	~SystemData();

	void LoadOBJFile(char* fileName, Microsoft::WRL::ComPtr<ID3D12Device> device, char* subSystemName);

public: //change
	uint16_t currentBaseLocation;

	uint32_t* indices;

	XMFLOAT3* positions;
	XMFLOAT4* colors;
	XMFLOAT3* normals;
	XMFLOAT2* uvs;

	std::unordered_map<char*, SubSystem> subSystemData;
};

