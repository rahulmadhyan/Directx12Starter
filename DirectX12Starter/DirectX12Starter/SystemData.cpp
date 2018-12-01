#include "SystemData.h"
#include <fstream>

SystemData::SystemData()
{
	currentBaseVertexLocation = 0;
	currentBaseIndexLocation = 0;
	
	indices = new uint16_t[UINT16_MAX];

	positions = new XMFLOAT3[UINT16_MAX];
	normals = new XMFLOAT3[UINT16_MAX];
	uvs = new XMFLOAT3[UINT16_MAX];

	worldPositions = new XMFLOAT3[UINT16_MAX];
	memset(worldPositions, 0, sizeof(XMFLOAT3) * UINT16_MAX);
	worldRotations = new XMFLOAT3[UINT16_MAX];
	memset(worldRotations, 0, sizeof(XMFLOAT3) * UINT16_MAX);
	worldScales = new XMFLOAT3[UINT16_MAX];
	memset(worldScales, 0, sizeof(XMFLOAT3) * UINT16_MAX);
	
	worldMatrices = new XMFLOAT4X4[UINT16_MAX];
}

SystemData::~SystemData()
{
	delete[] indices;
	indices = 0;

	delete[] positions;
	positions = 0;

	delete[] normals;
	normals = 0;

	delete[] uvs;
	uvs = 0;

	delete[] worldPositions;
	worldPositions = 0;

	delete[] worldRotations;
	worldRotations = 0;

	delete[] worldScales;
	worldScales = 0;

	delete[] worldMatrices;
	worldMatrices = 0;
}

const uint16_t SystemData::GetCurrentBaseVertexLocation()
{
	return currentBaseVertexLocation;
}

const uint16_t SystemData::GetCurrentBaseIndexLocation()
{
	return currentBaseIndexLocation;
}

const uint16_t* SystemData::GetIndices() 
{
	return indices;
}

const XMFLOAT3* SystemData::GetPositions() 
{
	return positions;
}

const XMFLOAT3* SystemData::GetNormals()
{
	return normals;
}

const XMFLOAT3* SystemData::GetUVs()
{
	return uvs;
}

SubmeshGeometry SystemData::GetSubSystem(char* subSystemName) const
{
	return subSystemData.at(subSystemName);
}

const XMFLOAT3* SystemData::GetWorldPosition(UINT index)
{
	return &worldPositions[index];
}

const XMFLOAT3* SystemData::GetWorldRotation(UINT index)
{
	return &worldRotations[index];
}

const XMFLOAT3* SystemData::GetWorldScale(UINT index)
{
	return &worldScales[index];
}

const XMFLOAT4X4* SystemData::GetWorldMatrix(UINT index)
{
	return &worldMatrices[index];
}

void SystemData::SetTranslation(UINT worldIndex, float x, float y, float z)
{
	XMVECTOR newPosition = XMVectorSet(worldPositions[worldIndex].x + x, worldPositions[worldIndex].y + y, worldPositions[worldIndex].z + z, 0.0f);
	XMStoreFloat3(&worldPositions[worldIndex], newPosition);
}

void SystemData::SetRotation(UINT worldIndex, float roll, float pitch, float yaw)
{
	XMVECTOR newRotation = XMVectorSet(worldRotations[worldIndex].x + roll, worldRotations[worldIndex].y + pitch, worldRotations[worldIndex].z + yaw, 0.0f);
	XMStoreFloat3(&worldRotations[worldIndex], newRotation);
}

void SystemData::SetScale(UINT worldIndex, float xScale, float yScale, float zScale)
{
	XMVECTOR newScale = XMVectorSet(worldScales[worldIndex].x + xScale, worldScales[worldIndex].y + yScale, worldScales[worldIndex].z + zScale, 0.0f);
	XMStoreFloat3(&worldScales[worldIndex], newScale);
}

void SystemData::SetWorldMatrix(UINT worldIndex)
{
	XMFLOAT3 currentWorldScale = worldScales[worldIndex];
	XMFLOAT3 currentWorldRotation = worldRotations[worldIndex];
	XMFLOAT3 currentWorldPosition = worldPositions[worldIndex];

	XMStoreFloat4x4(&worldMatrices[worldIndex], XMMatrixScaling(currentWorldScale.x, currentWorldScale.y, currentWorldScale.z) *
		XMMatrixRotationRollPitchYaw(currentWorldRotation.x, currentWorldRotation.y, currentWorldRotation.z) *
		XMMatrixTranslation(currentWorldPosition.x, currentWorldPosition.y, currentWorldPosition.z));
}

void SystemData::LoadOBJFile(char* fileName, Microsoft::WRL::ComPtr<ID3D12Device> device, char* subSystemName)
{
	SubmeshGeometry newSubSystem;
	newSubSystem.BaseVertexLocation = currentBaseVertexLocation;
	newSubSystem.StartIndexLocation = currentBaseIndexLocation;

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(fileName, aiProcess_ConvertToLeftHanded | aiProcess_Triangulate);

	unsigned int totalVertexCount = 0;
	unsigned int indexCounter = 0;

	for (size_t currentMesh = 0; currentMesh < scene->mNumMeshes; currentMesh++)
	{
		const aiMesh* mesh = scene->mMeshes[currentMesh];

		aiVector3D* objPositions = mesh->mVertices;
		aiVector3D* objNormals = mesh->mNormals;
		aiVector3D* objUVs = mesh->mTextureCoords[0];
		unsigned int vertexCounter = mesh->mNumVertices;
		
		XMFLOAT3* castPositions = reinterpret_cast<XMFLOAT3*>(&objPositions[0]);
		if (castPositions != NULL)
			memcpy(positions + currentBaseVertexLocation, castPositions, sizeof(XMFLOAT3) * vertexCounter);
		XMFLOAT3* castNormals = reinterpret_cast<XMFLOAT3*>(&objNormals[0]);
		if (castNormals != NULL)
			memcpy(normals + currentBaseVertexLocation, castNormals, sizeof(XMFLOAT3) * vertexCounter);
		XMFLOAT3* castUVs = reinterpret_cast<XMFLOAT3*>(&objUVs[0]);
		if (castUVs != NULL)
			memcpy(uvs + currentBaseVertexLocation, castUVs, sizeof(XMFLOAT3) * vertexCounter);

		aiFace* face = mesh->mFaces;
		unsigned int numberFaces = mesh->mNumFaces;
		for (size_t i = 0; i < numberFaces; i++)
		{
			aiFace currentFace = face[i];
			unsigned int currentNumberIndices = currentFace.mNumIndices;
			for (size_t j = 0; j < currentNumberIndices; j++)
			{
				indices[currentBaseIndexLocation] = totalVertexCount + currentFace.mIndices[j];
				currentBaseIndexLocation++;
				indexCounter++;
			}
		}

		currentBaseVertexLocation += vertexCounter;
		totalVertexCount += vertexCounter;
	}

	newSubSystem.IndexCount = indexCounter;

	XMFLOAT3* meshPositions = positions + newSubSystem.BaseVertexLocation;
	BoundingOrientedBox::CreateFromPoints(newSubSystem.Bounds, totalVertexCount, meshPositions, sizeof(XMFLOAT3));

	subSystemData[subSystemName] = newSubSystem;
}