#include "SystemDataOOP.h"

SystemDataOOP::SystemDataOOP()
{
}

SystemDataOOP::~SystemDataOOP()
{
}

const uint16_t SystemDataOOP::GetCurrentBaseVertexLocation()
{
	return currentBaseVertexLocation;
}

const uint16_t SystemDataOOP::GetCurrentBaseIntexLocation()
{
	return currentBaseIndexLocation;
}

const uint16_t* SystemDataOOP::GetIndices()
{
	return indices;
}

const XMFLOAT3 SystemDataOOP::GetPosition()
{
	return position;
}

const XMFLOAT3 SystemDataOOP::GetNormal()
{
	return normal;
}

const XMFLOAT3 SystemDataOOP::GetUV()
{
	return uv;
}

SubmeshGeometry SystemDataOOP::GetSubSystem(const char * subSystemName) const
{
	return subSystemData.at(subSystemName);
}

const XMFLOAT3 SystemDataOOP::GetWorldPosition()
{
	return worldPosition;
}

const XMFLOAT3 SystemDataOOP::GetWorldRotation()
{
	return worldRotation;
}

const XMFLOAT3 SystemDataOOP::GetWorldScale()
{
	return worldScale;
}

const XMFLOAT4X4 SystemDataOOP::GetWorldMatrix()
{
	return worldMatrix;
}

void SystemDataOOP::SetTranslation(float x, float y, float z)
{
	XMVECTOR newPosition = XMVectorSet(worldPosition.x + x, worldPosition.y + y, worldPosition.z + z, 0.0f);
	XMStoreFloat3(&worldPosition, newPosition);
}

void SystemDataOOP::SetRotation(float roll, float pitch, float yaw)
{
	XMVECTOR newRotation = XMVectorSet(worldRotation.x + roll, worldRotation.y + pitch, worldRotation.z + yaw, 0.0f);
	XMStoreFloat3(&worldRotation, newRotation);
}

void SystemDataOOP::SetScale(float xScale, float yScale, float zScale)
{
	XMVECTOR newScale = XMVectorSet(worldScale.x + xScale, worldScale.y + yScale, worldScale.z + zScale, 0.0f);
	XMStoreFloat3(&worldScale, newScale);
}

void SystemDataOOP::SetWorldMatrix()
{
	XMFLOAT3 currentWorldScale = worldScale;
	XMFLOAT3 currentWorldRotation = worldRotation;
	XMFLOAT3 currentWorldPosition = worldPosition;

	XMStoreFloat4x4(&worldMatrix, XMMatrixScaling(currentWorldScale.x, currentWorldScale.y, currentWorldScale.z) *
		XMMatrixRotationRollPitchYaw(currentWorldRotation.x, currentWorldRotation.y, currentWorldRotation.z) *
		XMMatrixTranslation(currentWorldPosition.x, currentWorldPosition.y, currentWorldPosition.z));
}