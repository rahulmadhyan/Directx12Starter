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

class SystemDataOOP
{
public:
	SystemDataOOP();
	~SystemDataOOP();

	const uint16_t GetCurrentBaseVertexLocation();
	const uint16_t GetCurrentBaseIntexLocation();

	const uint16_t* GetIndices();

	const XMFLOAT3 GetPosition();
	const XMFLOAT3 GetNormal();
	const XMFLOAT3 GetUV();

	SubmeshGeometry GetSubSystem(const char* subSystemName) const;

	const XMFLOAT3 GetWorldPosition();
	const XMFLOAT3 GetWorldRotation();
	const XMFLOAT3 GetWorldScale();
	const XMFLOAT4X4 GetWorldMatrix();

	void SetTranslation(float x, float y, float z);
	void SetRotation(float roll, float pitch, float yaw);
	void SetScale(float xScale, float yScale, float zScale);

	void SetWorldMatrix();

private:
	uint16_t currentBaseVertexLocation;
	uint16_t currentBaseIndexLocation;

	uint16_t* indices;

	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT3 uv;

	XMFLOAT3 worldPosition;
	XMFLOAT3 worldRotation;
	XMFLOAT3 worldScale;

	XMFLOAT4X4 worldMatrix;

	std::unordered_map<const char*, SubmeshGeometry> subSystemData;
};