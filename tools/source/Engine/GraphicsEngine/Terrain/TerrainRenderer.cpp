#include <stdafx.h>
#include "TerrainRenderer.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
#include <GraphicsEngine/GraphicsFrameworks/DX11/DX11Framework.h>
#include <GraphicsEngine/GraphicsFrameworks/DX11/DX11FullscreenTextureFactory.h>
#include <GraphicsEngine/Camera/Camera.h>
#include <Physics/PhysicsWorld.h>
#include <Input/InputManager.h>
#include <EngineSettings.h>

// Defined in TerrainTextureLoader.cpp (no PCH, contains stb_image)
extern ID3D11ShaderResourceView* TerrainLoadTexturePNG(const std::string& path, int slot, bool isNormal, ID3D11Device* device, ID3D11DeviceContext* context);

bool TerrainRenderer::Init()
{
	ID3D11Device*        Device  = DX11Framework::Device;
	ID3D11DeviceContext* Context = DX11Framework::Context;
	HRESULT result;

	// Frame buffer (b0)
	{
		D3D11_BUFFER_DESC desc = { 0 };
		desc.Usage          = D3D11_USAGE_DYNAMIC;
		desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.ByteWidth      = sizeof(TerrainFrameBufferData);
		result = Device->CreateBuffer(&desc, nullptr, &myTerrainFrameBuffer);
		if (FAILED(result)) return false;
	}

	// Sky buffer (b2)
	{
		D3D11_BUFFER_DESC desc = { 0 };
		desc.Usage          = D3D11_USAGE_DYNAMIC;
		desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.ByteWidth      = sizeof(TerrainSkyBufferData);
		result = Device->CreateBuffer(&desc, nullptr, &myTerrainSkyBuffer);
		if (FAILED(result)) return false;
	}

	// Linear-wrap sampler → s0
	{
		D3D11_SAMPLER_DESC sd = {};
		sd.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sd.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
		sd.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
		sd.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
		sd.MaxLOD         = D3D11_FLOAT32_MAX;
		sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		result = Device->CreateSamplerState(&sd, &mySampler);
		if (FAILED(result)) return false;
		Context->PSSetSamplers(0, 1, &mySampler);
		Context->VSSetSamplers(0, 1, &mySampler);
	}

	// Front-face culling rasterizer (used for reflection pass)
	{
		D3D11_RASTERIZER_DESC rd = {};
		rd.CullMode = D3D11_CULL_FRONT;
		rd.FillMode = D3D11_FILL_SOLID;
		result = Device->CreateRasterizerState(&rd, &myFrontFaceCulling);
		if (FAILED(result)) return false;
	}

	// Wireframe rasterizer for collision mesh debug view (toggled with P)
	{
		D3D11_RASTERIZER_DESC rd = {};
		rd.CullMode = D3D11_CULL_NONE;
		rd.FillMode = D3D11_FILL_WIREFRAME;
		result = Device->CreateRasterizerState(&rd, &myWireframeState);
		if (FAILED(result)) return false;
	}

	// Wireframe rasterizer with depth bias for debug overlay
	{
		D3D11_RASTERIZER_DESC rd = {};
		rd.CullMode = D3D11_CULL_NONE;
		rd.FillMode = D3D11_FILL_WIREFRAME;
		rd.DepthBias = -64;
		rd.SlopeScaledDepthBias = -2.0f;
		result = Device->CreateRasterizerState(&rd, &myDebugOverlayState);
		if (FAILED(result)) return false;
	}

	// depth test on, depth write off
	{
		D3D11_DEPTH_STENCIL_DESC dsd = {};
		dsd.DepthEnable    = TRUE;
		dsd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
		dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		result = Device->CreateDepthStencilState(&dsd, &myDebugDepthState);
		if (FAILED(result)) return false;
	}

	// Compile a minimal solid-green pixel shader at runtime so no external
	// .cso file is needed.
	{
		static const char kDebugPSSrc[] =
			"float4 main() : SV_Target { return float4(0.0f, 1.0f, 0.0f, 1.0f); }";
		ID3DBlob* psBlob  = nullptr;
		ID3DBlob* errBlob = nullptr;
		D3DCompile(kDebugPSSrc, sizeof(kDebugPSSrc) - 1, nullptr, nullptr, nullptr,
		           "main", "ps_4_0", 0, 0, &psBlob, &errBlob);
		if (psBlob)
		{
			Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
			                          nullptr, &myDebugPS);
			psBlob->Release();
		}
		if (errBlob) errBlob->Release();
	}

	// Reflection render target + depth
	Vector2ui res = DX11Framework::GetResolution();
	myReflectionRT    = DX11FullscreenTextureFactory::CreateTexture(res, DXGI_FORMAT_R8G8B8A8_UNORM);
	myReflectionDepth = DX11FullscreenTextureFactory::CreateDepth(res, DXGI_FORMAT_D32_FLOAT);

	const std::string& shaders = Settings::paths::shaders;
	const std::string& assets  = Settings::paths::engine_assets;
	const std::string texDir   = assets + "/Textures/Terrain/";

	// Init terrain
	if (!myTerrain.Initialize(Device, myTerrainParams)) return false;
	myTerrain.SetVertexShader(shaders + "/TerrainVS.cso");
	myTerrain.SetPixelShader(shaders + "/TerrainPS.cso");

	// Register terrain mesh with physics
	RegisterTerrainMesh();

	// Init water
	if (!myWater.Initialize(Device)) return false;
	myWater.SetVertexShader(shaders + "/WaterVS.cso");
	myWater.SetPixelShader(shaders + "/WaterPS.cso");
	myWater.FlattenGround(myTerrainParams.yOffset);

	// Load terrain textures
	LoadTexture(texDir + "Grass_c.png", 2, false);
	LoadTexture(texDir + "Grass_n.png", 3, true);
	LoadTexture(texDir + "Rock_c.png",  4, false);
	LoadTexture(texDir + "Rock_n.png",  5, true);
	LoadTexture(texDir + "Snow_c.png",  6, false);
	LoadTexture(texDir + "Snow_n.png",  7, true);
	LoadTexture(texDir + "Grass_c.png", 9,  false);
	LoadTexture(texDir + "Snow_n.png",  10, true);
	LoadTexture(texDir + "Rock_m.png",  11, true);

	// Fallback cubemap at t8
	if (!CreateFallbackCubemap()) return false;
	Context->PSSetShaderResources(8, 1, &myFallbackCubeSRV);

	return true;
}

bool TerrainRenderer::LoadTexture(const std::string& path, int slot, bool isNormal)
{
	ID3D11ShaderResourceView* srv = TerrainLoadTexturePNG(path, slot, isNormal, DX11Framework::Device, DX11Framework::Context);
	if (!srv) return false;
	myTextureSRVs.push_back({ slot, srv });
	return true;
}

bool TerrainRenderer::CreateFallbackCubemap()
{
	ID3D11Device* Device = DX11Framework::Device;

	// Create a 1x1x6 black cubemap texture
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width              = 1;
	desc.Height             = 1;
	desc.MipLevels          = 1;
	desc.ArraySize          = 6;
	desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count   = 1;
	desc.Usage              = D3D11_USAGE_DEFAULT;
	desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
	desc.MiscFlags          = D3D11_RESOURCE_MISC_TEXTURECUBE;

	uint32_t blackPixel = 0x00000000;
	D3D11_SUBRESOURCE_DATA initData[6];
	for (int i = 0; i < 6; i++)
	{
		initData[i].pSysMem          = &blackPixel;
		initData[i].SysMemPitch      = 4;
		initData[i].SysMemSlicePitch = 4;
	}

	ID3D11Texture2D* cubeTex = nullptr;
	HRESULT result = Device->CreateTexture2D(&desc, initData, &cubeTex);
	if (FAILED(result)) return false;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format                    = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MipLevels     = 1;
	srvDesc.TextureCube.MostDetailedMip = 0;

	result = Device->CreateShaderResourceView(cubeTex, &srvDesc, &myFallbackCubeSRV);
	cubeTex->Release();
	return SUCCEEDED(result);
}

void TerrainRenderer::RegenerateTerrain()
{
	myTerrain.Rebuild(myTerrainParams);
	myWater.GetTransform()(4, 2) = myTerrainParams.yOffset;
	// Re-register the collision mesh so physics sees the new heights immediately.
	RegisterTerrainMesh();
}

void TerrainRenderer::RegisterTerrainMesh()
{
	PhysicsWorld::TerrainCollisionMesh mesh;
	mesh.heightData = myTerrain.GetHeightDataPtr();
	mesh.width      = myTerrain.GetTerrainWidthCells();
	mesh.height     = myTerrain.GetTerrainHeightCells();
	mesh.offsetX    = myTerrain.GetWorldOffsetX();
	mesh.offsetY    = myTerrain.GetWorldOffsetY();
	mesh.offsetZ    = myTerrain.GetWorldOffsetZ();
	PhysicsWorld::Get().SetTerrainMesh(mesh);
}

void TerrainRenderer::Render(const Camera* camera, float totalTime, DX11FullscreenTexture* mainRT, DX11FullscreenTexture* mainDepth)
{
	ID3D11DeviceContext* Context = DX11Framework::Context;
	const std::string& shaders = Settings::paths::shaders;
	static bool sAppliedAutotestDebugDefault = false;
	if (!sAppliedAutotestDebugDefault)
	{
		sAppliedAutotestDebugDefault = true;
		char* envValue = nullptr;
		size_t envLen = 0;
		const errno_t err = _dupenv_s(&envValue, &envLen, "TE_AUTOTEST_SHOW_COLLISION");
		if (err == 0 && envValue && envValue[0] != '\0' && envValue[0] != '0')
		{
			myDebugCollisionMesh = true;
		}
		if (envValue) free(envValue);
	}

	// Toggle collision-mesh debug view with P
	if (InputManager::Get().IsKeyPressed('P'))
		myDebugCollisionMesh = !myDebugCollisionMesh;

	// Rebind sampler and all textures every frame (ForwardRenderer may have displaced them)
	Context->PSSetSamplers(0, 1, &mySampler);
	Context->VSSetSamplers(0, 1, &mySampler);
	for (auto& [slot, srv] : myTextureSRVs)
		Context->PSSetShaderResources((UINT)slot, 1, &srv);
	Context->PSSetShaderResources(8, 1, &myFallbackCubeSRV);

	// Upload terrain frame buffer
	{
		Matrix4x4f viewMatrix    = Matrix4x4f::GetFastInverse(camera->GetTransform().GetMatrix());
		Matrix4x4f worldToClip   = viewMatrix * camera->get_projection();
		Vector3f   camPos        = camera->GetTransform().GetPosition();

		TerrainFrameBufferData fb = {};
		fb.worldToClipMatrix = worldToClip;
		fb.deltaTime[0]      = totalTime;
		fb.CameraPosition    = Vector4f(camPos.X, camPos.Y, camPos.Z, 1.0f);

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		Context->Map(myTerrainFrameBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		memcpy(mapped.pData, &fb, sizeof(fb));
		Context->Unmap(myTerrainFrameBuffer, 0);
		Context->VSSetConstantBuffers(0, 1, &myTerrainFrameBuffer);
		Context->PSSetConstantBuffers(0, 1, &myTerrainFrameBuffer);
	}

	// Upload sky buffer
	{
		TerrainSkyBufferData sky = {};
		sky.dirLight      = myLightDir;
		sky.dirLightColour = myLightColour;
		sky.skyColour     = mySkyColour;
		sky.groundColour  = myGroundColour;

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		Context->Map(myTerrainSkyBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		memcpy(mapped.pData, &sky, sizeof(sky));
		Context->Unmap(myTerrainSkyBuffer, 0);
		Context->VSSetConstantBuffers(2, 1, &myTerrainSkyBuffer);
		Context->PSSetConstantBuffers(2, 1, &myTerrainSkyBuffer);
	}

	// Pass 1: Reflection
	myReflectionRT.SetAsActiveTarget(&myReflectionDepth);
	myReflectionRT.ClearTexture(Vector4f(0.2f, 0.2f, 0.6f, 1.0f));
	myReflectionDepth.ClearDepth();

	Context->RSSetState(myFrontFaceCulling);
	myTerrain.FlipMatrix();
	myTerrain.SetClipHeight(0.1f);
	myTerrain.SetPixelShader(shaders + "/TerrainReflectionPS.cso");
	myTerrain.Render(Context);
	myTerrain.FlipBackMatrix();
	myTerrain.SetPixelShader(shaders + "/TerrainPS.cso");

	// Restore main render target before drawing terrain + water to the visible scene
	mainRT->SetAsActiveTarget(mainDepth);

	// Pass 2: Main terrain
	Context->RSSetState(nullptr);
	myTerrain.SetClipHeight(-9999.0f);
	myTerrain.Render(Context);

	// Pass 3: Water
	if (myDrawWater)
	{
		ID3D11ShaderResourceView* reflSRV = myReflectionRT.GetSRV();
		Context->PSSetShaderResources(12, 1, &reflSRV);
		myWater.Render(Context);
		ID3D11ShaderResourceView* nullSRV = nullptr;
		Context->PSSetShaderResources(12, 1, &nullSRV);
	}

	// Pass 4 - Debug: collision mesh wireframe overlay (toggle with P)
	if (myDebugCollisionMesh)
	{
		Context->OMSetDepthStencilState(myDebugDepthState, 0);
		Context->RSSetState(myDebugOverlayState);
		myTerrain.SetClipHeight(-9999.0f);
		myTerrain.Render(Context, myDebugPS);
		// Restore default pipeline state
		Context->OMSetDepthStencilState(nullptr, 0);
		Context->RSSetState(nullptr);
	}
}

TerrainRenderer::~TerrainRenderer()
{
	if (myTerrainFrameBuffer) myTerrainFrameBuffer->Release();
	if (myTerrainSkyBuffer)   myTerrainSkyBuffer->Release();
	if (mySampler)            mySampler->Release();
	if (myFrontFaceCulling)   myFrontFaceCulling->Release();
	if (myWireframeState)     myWireframeState->Release();
	if (myDebugOverlayState)  myDebugOverlayState->Release();
	if (myDebugDepthState)    myDebugDepthState->Release();
	if (myDebugPS)            myDebugPS->Release();
	if (myFallbackCubeSRV)    myFallbackCubeSRV->Release();

	for (auto& [slot, srv] : myTextureSRVs)
	{
		if (srv) srv->Release();
	}
}
