#include <stdafx.h>

#include "DX11FullscreenTextureFactory.h"

#include <DX11Framework.h>

DX11FullscreenTexture DX11FullscreenTextureFactory::CreateTexture(Vector2ui aSize, DXGI_FORMAT aFormat)
{
	HRESULT result;
	D3D11_TEXTURE2D_DESC desc = { 0 };
	desc.Width = aSize.X;
	desc.Height = aSize.Y;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = aFormat;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	ID3D11Texture2D* texture;
	result = DX11Framework::Device->CreateTexture2D(&desc, nullptr, &texture);
	assert(SUCCEEDED(result));

	DX11FullscreenTexture textureResult = CreateTexture(texture);

	ID3D11ShaderResourceView* SRV;
	result = DX11Framework::Device->CreateShaderResourceView(texture, nullptr, &SRV);
	assert(SUCCEEDED(result));

	textureResult.mySRV = SRV;
	return textureResult;
}

DX11FullscreenTexture DX11FullscreenTextureFactory::CreateTexture(ID3D11Texture2D* aTextureTemplate)
{
	HRESULT result;

	ID3D11RenderTargetView* RTV;
	result = DX11Framework::Device->CreateRenderTargetView(
		aTextureTemplate,
		nullptr,
		&RTV);
	assert(SUCCEEDED(result));

	D3D11_VIEWPORT* viewport = nullptr;
	if (aTextureTemplate)
	{
		D3D11_TEXTURE2D_DESC desc;
		aTextureTemplate->GetDesc(&desc);
		viewport = new D3D11_VIEWPORT(
			{
				0,
				0,
				static_cast<float>(desc.Width),
				static_cast<float>(desc.Height),
				0,
				1
			});
	}

	DX11FullscreenTexture textureResult;
	textureResult.myTexture = aTextureTemplate;
	textureResult.myTargetTextureType.myRenderTarget = RTV;
	textureResult.myViewport = viewport;
	return textureResult;
}

DX11FullscreenTexture DX11FullscreenTextureFactory::CreateDepth(Vector2ui aSize, DXGI_FORMAT aFormat)
{
	HRESULT result;
	D3D11_TEXTURE2D_DESC desc = { 0 };
	desc.Width = aSize.X;
	desc.Height = aSize.Y;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = aFormat;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	ID3D11Texture2D* texture;
	result = DX11Framework::Device->CreateTexture2D(&desc, nullptr, &texture);
	assert(SUCCEEDED(result));

	ID3D11DepthStencilView* DSV;
	result = DX11Framework::Device->CreateDepthStencilView(texture, nullptr, &DSV);
	assert(SUCCEEDED(result));

	D3D11_VIEWPORT* viewport = new D3D11_VIEWPORT(
		{
			0,
			0,
			static_cast<float>(aSize.X),
			static_cast<float>(aSize.Y),
			0,
			1
		});

	DX11FullscreenTexture textureResult;
	textureResult.myTexture = texture;
	textureResult.myTargetTextureType.myDepth = DSV;
	textureResult.myViewport = viewport;
	return textureResult;
}
