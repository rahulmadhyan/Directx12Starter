#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <DirectXMath.h>
#include <d3d12.h>
#include "Vertex.h"
#include "wrl.h"

using namespace DirectX;

struct SubSystem
{
	uint32_t baseLocation;
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

	const uint16_t GetCurrentBaseLocation();
	
	const uint16_t* GetIndices();

	const XMFLOAT3* GetPositions();
	const XMFLOAT3* GetNormals();
	const XMFLOAT2* GetUVs();

	SubSystem GetSubSystem(char* subSystemName) const;

	const XMFLOAT3* GetWorldPosition(UINT index);
	const XMFLOAT3* GetWorldRotation(UINT index);
	const XMFLOAT3* GetWorldScale(UINT index);
	const XMFLOAT4X4* GetWorldMatrix(UINT index);

	void SetTranslation(UINT worldIndex, float x, float y, float z);
	void SetRotation(UINT worldIndex, float roll, float pitch, float yaw);
	void SetScale(UINT worldIndex, float xScale, float yScale, float zScale);

	void SetWorldMatrix(UINT worldIndex);

	void LoadOBJFile(char* fileName, Microsoft::WRL::ComPtr<ID3D12Device> device, char* subSystemName);
private:
	uint16_t currentBaseLocation;

	uint16_t* indices;

	XMFLOAT3* positions;
	XMFLOAT3* normals;
	XMFLOAT2* uvs;

	XMFLOAT3* worldPositions;
	XMFLOAT3* worldRotations;
	XMFLOAT3* worldScales;

	XMFLOAT4X4* worldMatrices;

	std::unordered_map<char*, SubSystem> subSystemData;
};

