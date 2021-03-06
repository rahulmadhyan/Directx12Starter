#pragma once
#include "DXCore.h"
#include "Camera.h"
#include "InputManager.h"
#include "FrameResource.h"
#include "Entity.h"
#include "GeometryGenerator.h"
#include "SystemData.h"
#include "DDSTextureLoader.h"
#include "WICTextureLoader.h"
#include "Player.h"
#include "Enemies.h"
#include "Ray.h"
#include "Emitter.h"

#ifdef _DEBUG
#include <DirectXColors.h>
#include "DebugDraw.h"
#include "GraphicsMemory.h"
#include "PrimitiveBatch.h"
#include "RenderTargetState.h"
#include "Effects.h"
#include "CommonStates.h"
#include "EffectPipelineStateDescription.h"
#include "VertexTypes.h"
#endif

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

class Game : public DXCore
{
public:
	Game(HINSTANCE hInstance);
	Game(const Game& rhs) = delete;
	Game& operator=(const Game& rhs) = delete;
	~Game();

	virtual bool Initialize()override;

private:
#ifdef _DEBUG
	std::unique_ptr<DirectX::GraphicsMemory> graphicsMemory;
	std::unique_ptr<DirectX::BasicEffect> effect;
	std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> batch;

	void InitDebugDraw();
	void DebugDraw(ID3D12GraphicsCommandList* cmdList, const std::vector<Entity*> entities);
	void DebugDrawPlayerRay(ID3D12GraphicsCommandList* cmdList, const std::vector<Entity*> entities);
#endif // DEBUG

	std::vector<std::unique_ptr<FrameResource>> FrameResources;
	FrameResource* currentFrameResource = nullptr;
	int currentFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	
	ComPtr<ID3D12DescriptorHeap> CBVHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> matCBVHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> SRVHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> Geometries;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> Shaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> PSOs;
	std::unordered_map<std::string, std::unique_ptr<Material>> Materials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> Textures;
	std::unordered_map<std::string, std::unique_ptr<Texture>> CubeMapTextures;

	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> particleInputLayout;

	// list of all the entities
	std::vector<std::unique_ptr<Entity>> allEntities;

	std::vector<Entity*> playerEntities;
	std::vector<Entity*> sceneEntities;
	std::vector<Entity*> enemyEntities;
	std::vector<Entity*> emitterEntities;
	std::vector<Entity*> skyEntities;

	PassConstants MainPassCB;

	UINT PassCbvOffset = 0;

	Camera mainCamera;

	InputManager* inputManager;

	SystemData *systemData;

	Player *player;

	Enemies *enemies;

	float mSunTheta = 1.25f * XM_PIDIV2;
	float mSunPhi = XM_PIDIV4;

	virtual void Resize()override;
	virtual void Update(const Timer& timer)override;
	virtual void Draw(const Timer& timer)override;

	void UpdateObjectCBs(const Timer& timer);
	void UpdateMainPassCB(const Timer& timer);
	void UpadteMaterialCBs(const Timer& timet);

	void BuildTextures();
	void BuildDescriptorHeaps();
	void BuildConstantBufferViews();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildEntities();
	void DrawEntities(ID3D12GraphicsCommandList* cmdList, const std::vector<Entity*> entities);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
};

