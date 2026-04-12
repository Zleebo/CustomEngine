#include <stdafx.h>

#include <DX11Framework.h>
#include <Win32Window.h>

ID3D11Device* DX11Framework::Device;
ID3D11DeviceContext* DX11Framework::Context;
std::shared_ptr<WindowHandler> DX11Framework::myWindowHandler;
IDXGISwapChain* DX11Framework::SwapChain;
ID3D11DepthStencilView* DX11Framework::DepthBuffer;
ID3D11RenderTargetView* DX11Framework::BackBuffer;

DX11Framework::DX11Framework()
{
	SwapChain = nullptr;
	Device = nullptr;
	Context = nullptr;
	BackBuffer = nullptr;
}

DX11Framework::~DX11Framework()
{
	SwapChain = nullptr;
	Device = nullptr;
	Context = nullptr;
	BackBuffer = nullptr;
}

bool DX11Framework::Init(std::shared_ptr<WindowHandler> aWindowHandler)
{
	if (aWindowHandler)
	{
		myWindowHandler = aWindowHandler;
	}
	HRESULT result;

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = myWindowHandler->GetWindowHandle();
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = true;

	result = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&swapChainDesc,
		&SwapChain,
		&Device,
		nullptr,
		&Context
	);

	ID3D11Texture2D* backBufferTexture;
	result = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBufferTexture);
	if (FAILED(result))
	{
		return false;
	}

	result = Device->CreateRenderTargetView(backBufferTexture, nullptr, &BackBuffer);
	if (FAILED(result))
	{
		backBufferTexture->Release();
		return false;
	}

	result = backBufferTexture->Release();
	if (FAILED(result))
	{
		return false;
	}

	// TEMP
	ID3D11Texture2D* depthBufferTexture;
	D3D11_TEXTURE2D_DESC depthBufferDesc = { 0 };
	depthBufferDesc.Width = static_cast<unsigned int>(myWindowHandler->GetWidth());
	depthBufferDesc.Height = static_cast<unsigned int>(myWindowHandler->GetHeight());
	depthBufferDesc.ArraySize = 1;
	depthBufferDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthBufferDesc.SampleDesc.Count = 1;
	depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	result = Device->CreateTexture2D(&depthBufferDesc, nullptr, &depthBufferTexture);
	if (FAILED(result))
	{
		return false;
	}

	result = Device->CreateDepthStencilView(depthBufferTexture, nullptr, &DepthBuffer);
	if (FAILED(result))
	{
		return false;
	}

	Context->OMSetRenderTargets(1, &BackBuffer, DepthBuffer);

	D3D11_VIEWPORT viewport = { 0 };
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(myWindowHandler->GetWidth());
	viewport.Height = static_cast<float>(myWindowHandler->GetHeight());
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	Context->RSSetViewports(1, &viewport);
	//END TEMP

	return true;
}

void DX11Framework::BeginFrame(std::array<float, 4> aClearColor)
{
	//Context->ClearRenderTargetView(BackBuffer, &aClearColor[0]);
	//Context->ClearDepthStencilView(DepthBuffer, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void DX11Framework::EndFrame()
{
	SwapChain->Present(1, 0);
}

Vector2ui DX11Framework::GetResolution()
{
	return { 
		static_cast<uint16_t>(myWindowHandler->GetWidth()), 
		static_cast<uint16_t>(myWindowHandler->GetHeight()) 
	};
}

void DX11Framework::Resize(uint32_t aWidth, uint32_t aHeight)
{
	if (!SwapChain || aWidth == 0 || aHeight == 0)
		return;
	if (FAILED(SwapChain->ResizeBuffers(0, aWidth, aHeight, DXGI_FORMAT_UNKNOWN, 0)))
		return;
	myWindowHandler->SetWidth(static_cast<float>(aWidth));
	myWindowHandler->setHeight(static_cast<float>(aHeight));
}

/*Resize?

		if (aResolution.x == 0 || aResolution.y == 0)
	{
		return;
	}
	static float color[4];
	color[0] = 1;
	color[1] = 1;
	color[2] = 1;
	color[3] = 1;

	Clear(myClearColor);
	myWindowSize = aResolution;

	CEngine::GetInstance()->myWindowSize = aResolution;
	CEngine::GetInstance()->myRenderSize = aResolution;

	myDeviceContext->OMSetRenderTargets(0, 0, 0);
	myDeviceContext->OMSetDepthStencilState(0, 0);
	myDeviceContext->ClearState();
	if (myBackbuffer)
	{
		myBackbuffer.Reset();
	}
	if (myDepthStencilView)
	{
		myDepthStencilView.Reset();
	}
	if (myDepthStencilBuffer)
	{
		myDepthStencilBuffer.Reset();
	}
	if (myDepthStencilState)
	{
		myDepthStencilState.Reset();
	}
	if (!mySwapchain)
	{
		return;
	}

	if (mySwapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0) != S_OK)
	{
		ERROR_PRINT("%s", "Could not resize buffers!");
		return;
	}

	ID3D11Texture2D* pBuffer = nullptr;
	if (mySwapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBuffer) != S_OK)
	{
		ERROR_PRINT("%s", "Could not resize buffers!");
		return;
	}

	if (!pBuffer)
	{
		return;
	}
	if (myDevice->CreateRenderTargetView(pBuffer, NULL, myBackbuffer.ReleaseAndGetAddressOf()) != S_OK)
	{
		ERROR_PRINT("%s", "Could not resize buffers!");
		return;
	}


	pBuffer->Release();

	depthBufferDesc.Width = myWindowSize.x;
	depthBufferDesc.Height = myWindowSize.y;

	HRESULT result = myDevice->CreateTexture2D(&depthBufferDesc, NULL, myDepthStencilBuffer.ReleaseAndGetAddressOf());
	if (FAILED(result))
	{
		ERROR_PRINT("%s", "Create tex2d error");
		return;
	}

	result = myDevice->CreateDepthStencilView(myDepthStencilBuffer.Get(), &depthStencilViewDesc, myDepthStencilView.ReleaseAndGetAddressOf());
	if (FAILED(result))
	{
		ERROR_PRINT("%s", "depth stencil view error");
		return;
	}

	result = myDevice->CreateDepthStencilState(&depthStencilDesc, myDepthStencilState.ReleaseAndGetAddressOf());
	if (FAILED(result))
	{
		ERROR_PRINT("%s", "Depth stencil error");
		return;
	}

	myDeviceContext->RSSetState(myRasterState.Get());
	myDeviceContext->OMSetDepthStencilState(myDepthStencilState.Get(), 1);
	myDeviceContext->OMSetRenderTargets(1, myBackbuffer.GetAddressOf(), nullptr);

	SetViewPort(0, 0, (float)myWindowSize.x, (float)myWindowSize.y);
	*/