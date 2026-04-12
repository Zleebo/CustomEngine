#include <stdafx.h>
#include "GraphicsEngine.h"
#include <DX11Framework.h>
#include <ForwardRenderer.h>
#include <Win32Window.h>
#include <Particles/ParticleRenderer.h>
#include <GraphicsEngine/PostProcess/PostProcessor.h>
#include <UI/UIRenderer.h>

#include <DX11FullscreenTextureFactory.h>
#include <DDSTextureLoader11.h>
//#include <DirectXTex.h>
//#include <DirectXTex.inl>

#include <rapidjson/rapidjson.h>

#include <vector>
#include <fstream>

DX11FullscreenTexture GraphicsEngine::myIDBuffer;

GraphicsEngine::GraphicsEngine()
{
	myForwardRenderer	= std::shared_ptr<ForwardRenderer>(new ForwardRenderer());
	myFramework			= std::shared_ptr<DX11Framework>(new DX11Framework());
	myWindowHandler		= std::shared_ptr<WindowHandler>(new WindowHandler());
}

GraphicsEngine::~GraphicsEngine()
{
}

bool GraphicsEngine::Init()
{
	OutputDebugStringA("[GE] Init start\n");

	if(!myWindowHandler->Init(Settings::window_data))
	{
		OutputDebugStringA("[GE] FAIL: WindowHandler::Init\n");
		return false;
	}
	OutputDebugStringA("[GE] WindowHandler OK\n");

	if(!myFramework->Init(myWindowHandler))
	{
		OutputDebugStringA("[GE] FAIL: DX11Framework::Init\n");
		return false;
	}
	OutputDebugStringA("[GE] Framework OK\n");

	// Create render textures before subsystem inits
	{
		ID3D11Resource* backBufferResource = nullptr;
		DX11Framework::BackBuffer->GetResource(&backBufferResource);
		ID3D11Texture2D* backBufferTexture = reinterpret_cast<ID3D11Texture2D*>(backBufferResource);
		if (!backBufferTexture)
		{
			OutputDebugStringA("[GE] FAIL: backBufferTexture null\n");
			return false;
		}

		myBackbuffer      = DX11FullscreenTextureFactory::CreateTexture(backBufferTexture);
		myBackbufferDepth = DX11FullscreenTextureFactory::CreateDepth(DX11Framework::GetResolution(), DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT);
		myRenderTexture   = DX11FullscreenTextureFactory::CreateTexture(DX11Framework::GetResolution(), DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM);
		myIDBuffer        = DX11FullscreenTextureFactory::CreateTexture(DX11Framework::GetResolution(), DXGI_FORMAT::DXGI_FORMAT_R32_SINT);

		char buf[128];
		sprintf_s(buf, "[GE] Textures created. RenderTexture SRV=%p\n", myRenderTexture.GetSRV());
		OutputDebugStringA(buf);
	}

	if(!myForwardRenderer->Init())
	{
		OutputDebugStringA("[GE] FAIL: ForwardRenderer::Init\n");
		return false;
	}
	OutputDebugStringA("[GE] ForwardRenderer OK\n");

	myTerrainRenderer = std::make_unique<TerrainRenderer>();
	if (!myTerrainRenderer->Init())
	{
		OutputDebugStringA("[GE] FAIL: TerrainRenderer::Init\n");
		return false;
	}
	OutputDebugStringA("[GE] TerrainRenderer OK\n");

	ParticleRenderer::Get().Init();
	PostProcessor::Get().Init();
	UIRenderer::Get().Init();
	OutputDebugStringA("[GE] Init complete\n");

	return true;
}

int GraphicsEngine::MouseOver(uint32_t x, uint32_t y)
{
	if (!myIDBuffer.myTargetTextureType.myRenderTarget) return -1;

	ID3D11Resource *src;
	const auto& context = DX11Framework::Context;
	myIDBuffer.myTargetTextureType.myRenderTarget->GetResource(&src);

	D3D11_TEXTURE2D_DESC textureDesc;
	textureDesc.Width = 1;
	textureDesc.Height = 1;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = (DXGI_FORMAT_R32_SINT);
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	textureDesc.Usage = D3D11_USAGE_STAGING;
	textureDesc.BindFlags = 0;
	textureDesc.MiscFlags = 0;

	ID3D11Texture2D *tmp = nullptr;
	if (FAILED(DX11Framework::Device->CreateTexture2D(&textureDesc, nullptr, &tmp)) || !tmp)
		return -1;

	D3D11_BOX srcBox;
	srcBox.left = x;
	srcBox.right = x + 1;
	srcBox.bottom = y + 1;
	srcBox.top = y;
	srcBox.front = 0;
	srcBox.back = 1;
		
	DX11Framework::Context->CopySubresourceRegion(
		tmp, 
		0, 0, 0, 0,
		src, 0, 
		&srcBox
	);

	D3D11_MAPPED_SUBRESOURCE msr = {};
	context->Map(tmp, 0, D3D11_MAP::D3D11_MAP_READ, 0, &msr);
	int32_t* data = nullptr;
	if (x >= 0 && x <= DX11Framework::GetResolution().X-1 && y >= 0 && y <= DX11Framework::GetResolution().Y-1)
	{
		data = reinterpret_cast<int32_t*>(msr.pData);

	}
	
	context->Unmap(tmp, 0);
	tmp->Release();

	return (data!=nullptr) ? (int32_t)*data : -1;
}

void GraphicsEngine::BeginFrame() {	
	myBackbuffer.ClearTexture();
	myBackbufferDepth.ClearDepth();
	std::array<float, 4> clear_id{ -1.f, -1.f, -1.f, -1.f };
	myIDBuffer.ClearTexture(clear_id);

	if (myIsRuntime == false)
	{
		myRenderTexture.ClearTexture(Settings::window_data.clear_color);
	}

	myFramework->BeginFrame(Settings::window_data.clear_color);
}

void GraphicsEngine::EndFrame()
{
	myFramework->EndFrame();
}

void GraphicsEngine::RenderFrame()
{
	myIDBuffer.SetAsActiveTarget();
	myForwardRenderer->RenderID(myScene.GetActiveCamera(), myScene.CullModels());

	if (myIsRuntime)
	{
		myRenderTexture.ClearTexture(Settings::window_data.clear_color);
		myRenderTexture.SetAsActiveTarget(&myBackbufferDepth);
		myTerrainRenderer->Render(myScene.GetActiveCamera(), myTotalTime, &myRenderTexture, &myBackbufferDepth);
		myForwardRenderer->Render(myScene.GetActiveCamera(), myScene.CullModels());
		ParticleRenderer::Get().Render(myScene.GetActiveCamera(), {});

		myBackbuffer.SetAsActiveTarget();
		PostProcessor::Get().Process(myRenderTexture, myBackbuffer);

		UIRenderer::Get().BeginFrame();
		// game code can call UIRenderer::Get().DrawRect / DrawProgressBar here
		UIRenderer::Get().EndFrame();
	}
	else
	{
		myRenderTexture.SetAsActiveTarget(&myBackbufferDepth);
		myTerrainRenderer->Render(myScene.GetActiveCamera(), myTotalTime, &myRenderTexture, &myBackbufferDepth);
		myForwardRenderer->Render(myScene.GetActiveCamera(), myScene.CullModels());
	}
	myBackbuffer.SetAsActiveTarget();
}

void GraphicsEngine::Resize(uint32_t w, uint32_t h)
{
	if (!DX11Framework::SwapChain || w == 0 || h == 0)
		return;

	DX11Framework::Context->OMSetRenderTargets(0, nullptr, nullptr);

	myBackbuffer.Release();
	if (DX11Framework::BackBuffer)
	{
		DX11Framework::BackBuffer->Release();
		DX11Framework::BackBuffer = nullptr;
	}

	DX11Framework::Resize(w, h);

	{
		ID3D11Texture2D* backBufferTexture = nullptr;
		if (FAILED(DX11Framework::SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBufferTexture)))
			return;
		DX11Framework::Device->CreateRenderTargetView(backBufferTexture, nullptr, &DX11Framework::BackBuffer);
		myBackbuffer = DX11FullscreenTextureFactory::CreateTexture(backBufferTexture);
	}

	// recreate off-screen textures at new resolution
	const Vector2ui newSize = { w, h };
	myBackbufferDepth.Release();
	myBackbufferDepth = DX11FullscreenTextureFactory::CreateDepth(newSize, DXGI_FORMAT_D32_FLOAT);
	myRenderTexture.Release();
	myRenderTexture = DX11FullscreenTextureFactory::CreateTexture(newSize, DXGI_FORMAT_R8G8B8A8_UNORM);
	myIDBuffer.Release();
	myIDBuffer = DX11FullscreenTextureFactory::CreateTexture(newSize, DXGI_FORMAT_R32_SINT);
}
