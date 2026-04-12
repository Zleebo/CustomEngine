#pragma once
#include <array>
#include <d3d11.h>
#include <memory>

#include <Math/Vector.h>

class WindowHandler;
struct ModelData;

// DirectX 11 Framework. Shorthand to make it easier to deal with.
class DX11Framework
{
public:
	static IDXGISwapChain* SwapChain;
	static ID3D11DepthStencilView* DepthBuffer;
	static ID3D11RenderTargetView* BackBuffer;
	static ID3D11Device* Device;
	static ID3D11DeviceContext* Context;
	
public:
	DX11Framework();
	~DX11Framework();

	static bool Init(std::shared_ptr<WindowHandler> aWindowHandler = nullptr);
	void BeginFrame(std::array<float, 4> aClearColor);
	void EndFrame();

	static Vector2ui GetResolution();
	static void Resize(uint32_t aWidth, uint32_t aHeight);

private:
	static std::shared_ptr<WindowHandler> myWindowHandler;
};

