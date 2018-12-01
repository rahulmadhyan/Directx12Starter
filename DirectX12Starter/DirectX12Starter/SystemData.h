#pragma once
#include <cstdint>
#include <unordered_map>
#include <d3d12.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "d3dUtil.h"
#include "Vertex.h"
#include "wrl.h"

using namespace DirectX;

class SystemData
{
public:
	SystemData();
	~SystemData();

	const uint16_t GetCurrentBaseVertexLocation();
	const uint16_t GetCurrentBaseIndexLocation();
	
	const uint16_t* GetIndices();

	const XMFLOAT3* GetPositions();
	const XMFLOAT3* GetNormals();
	const XMFLOAT3* GetUVs();

	SubmeshGeometry GetSubSystem(char* subSystemName) const;

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
	uint16_t currentBaseVertexLocation;
	uint16_t currentBaseIndexLocation;

	uint16_t* indices;

	XMFLOAT3* positions;
	XMFLOAT3* normals;
	XMFLOAT3* uvs;

	XMFLOAT3* worldPositions;
	XMFLOAT3* worldRotations;
	XMFLOAT3* worldScales;

	XMFLOAT4X4* worldMatrices;

	std::unordered_map<char*, SubmeshGeometry> subSystemData;
};

