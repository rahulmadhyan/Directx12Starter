 #pragma once
#include <DirectXMath.h>
#include "MathHelper.h"
#include "d3dUtil.h"

using namespace DirectX;

extern const int gNumberFrameResources;

// lightweight structure stores parameters to draw a shape
// this will vary from app-to-app

struct Entity
{
	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumberFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumberFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	// Index into SystemData for worldPosition etc.
	UINT SystemWorldIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;

	/*XMFLOAT3 Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 Scale = XMFLOAT3(0.0f, 0.0f, 0.0f);*/

	//XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TextureTransform = MathHelper::Identity4x4();

	Entity() = default;

	/*void SetTranslation(float x, float y, float z)
	{
		XMVECTOR newPosition = XMVectorSet(Position.x + x, Position.y + y, Position.z + z, 0.0f);
		XMStoreFloat3(&Position, newPosition);
	}

	void SetRotation(float roll, float pitch, float yaw)
	{
		XMVECTOR newRotation = XMVectorSet(Rotation.x + roll, Rotation.y + pitch, Rotation.z + yaw, 0.0f);
		XMStoreFloat3(&Rotation, newRotation);
	}

	void SetScale(float xScale, float yScale, float zScale)
	{
		XMVECTOR newScale = XMVectorSet(Scale.x + xScale, Scale.y + yScale, Scale.z + zScale, 0.0f);
		XMStoreFloat3(&Scale, newScale);
	}

	void SetWorldMatrix()
	{
		XMStoreFloat4x4(&World, XMMatrixScaling(Scale.x, Scale.y, Scale.z) * 
			XMMatrixRotationRollPitchYaw(Rotation.x, Rotation.y, Rotation.z) * 
			XMMatrixTranslation(Position.x, Position.y,	Position.z));
	}*/
};