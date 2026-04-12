#pragma once
#include <vector>
#include <Math/Matrix4x4.h>
#include <Math/Vector.h>
#include <GraphicsEngine/GraphicsFrameworks/DX11/DX11FullscreenTexture.h>
#include "Terrain.h"

#include <GraphicsEngine/Camera/Camera.h>
struct ID3D11Buffer;
struct ID3D11SamplerState;
struct ID3D11RasterizerState;
struct ID3D11DepthStencilState;
struct ID3D11PixelShader;
struct ID3D11ShaderResourceView;

class TerrainRenderer
{
public:
	bool Init();
	void Render(const Camera* camera, float totalTime, DX11FullscreenTexture* mainRT, DX11FullscreenTexture* mainDepth);
	~TerrainRenderer();

	TerrainParams& GetParams()     { return myTerrainParams; }
	bool&          GetDrawWater()  { return myDrawWater; }
	void           RegenerateTerrain();
	float          GetTerrainHeightAt(float worldX, float worldZ) const { return myTerrain.GetHeightAt(worldX, worldZ); }

private:
	void RegisterTerrainMesh();

	struct TerrainFrameBufferData
	{
		Matrix4x4f worldToClipMatrix;
		float      deltaTime[4];
		Vector4f   CameraPosition;
	};
	struct TerrainSkyBufferData
	{
		Vector4f dirLight;
		Vector4f dirLightColour;
		Vector4f skyColour;
		Vector4f groundColour;
	};

	bool LoadTexture(const std::string& path, int slot, bool isNormal);
	bool CreateFallbackCubemap();

	ID3D11Buffer*             myTerrainFrameBuffer = nullptr;
	ID3D11Buffer*             myTerrainSkyBuffer   = nullptr;
	ID3D11SamplerState*       mySampler            = nullptr;
	ID3D11RasterizerState*    myFrontFaceCulling   = nullptr;
	ID3D11RasterizerState*    myWireframeState     = nullptr;
	ID3D11RasterizerState*    myDebugOverlayState  = nullptr;
	ID3D11DepthStencilState*  myDebugDepthState    = nullptr;  // depth test on, depth write off
	ID3D11PixelShader*        myDebugPS            = nullptr;  // solid green output
	ID3D11ShaderResourceView* myFallbackCubeSRV    = nullptr;

	std::vector<std::pair<int, ID3D11ShaderResourceView*>> myTextureSRVs;

	DX11FullscreenTexture myReflectionRT;
	DX11FullscreenTexture myReflectionDepth;

	Terrain       myTerrain;
	Terrain       myWater;
	TerrainParams myTerrainParams;

	bool myDebugCollisionMesh = false;
	bool myDrawWater          = false;

	Vector4f myLightDir     { 0.5f, 0.8f, 0.3f, 1.0f };
	Vector4f myLightColour  { 1.0f, 0.9f, 0.8f, 1.0f };
	Vector4f mySkyColour    { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4f myGroundColour { 72.f/255.f, 152.f/255.f, 47.f/255.f, 1.0f };
};
