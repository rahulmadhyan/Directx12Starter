 #pragma once
#include <DirectXMath.h>
#include <DirectXCollision.h>
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
	MeshGeometry* wireFrameGeo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	SubmeshGeometry meshData;

	//// DrawIndexedInstanced parameters.
	//UINT IndexCount = 0;
	//UINT StartIndexLocation = 0;
	//int BaseVertexLocation = 0;

	//BoundingOrientedBox boudingBox;

	XMFLOAT4X4 TextureTransform = MathHelper::Identity4x4();

	Entity() = default;
};

struct EnemyEntity : public Entity
{
	bool isRanged = false;
	int currentWaypointIndex = 0;
};