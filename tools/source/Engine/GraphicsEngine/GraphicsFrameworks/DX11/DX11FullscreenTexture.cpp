#include <stdafx.h>

#include "DX11FullscreenTexture.h"
#include <DX11Framework.h>

DX11FullscreenTexture::DX11FullscreenTexture()
{
	myTexture = nullptr;
	myTargetTextureType.myRenderTarget = nullptr;
	mySRV = nullptr;
	myViewport = nullptr;
}

DX11FullscreenTexture::~DX11FullscreenTexture()
{
	// Lifetime is managed manually via Release(). Do not call Release() here,
	// as these objects are copied/moved in vectors and the COM refs are shared.
}

void DX11FullscreenTexture::ClearTexture(const Vector4f &aClearColor)
{
	DX11Framework::Context->ClearRenderTargetView(myTargetTextureType.myRenderTarget, &aClearColor.X);
}

void DX11FullscreenTexture::ClearTexture(const std::array<float, 4>& aClearColor)
{
	DX11Framework::Context->ClearRenderTargetView(myTargetTextureType.myRenderTarget, &aClearColor[0]);
}

void DX11FullscreenTexture::ClearDepth(float aClearDepthValue, uint8_t aClearStencilValue)
{
	DX11Framework::Context->ClearDepthStencilView(myTargetTextureType.myDepth, D3D11_CLEAR_DEPTH | D3D11_CLEAR_DEPTH, aClearDepthValue, aClearStencilValue);
}

void DX11FullscreenTexture::SetAsActiveTarget(DX11FullscreenTexture* aDepth)
{
	if (aDepth)
	{
		DX11Framework::Context->OMSetRenderTargets(1, &myTargetTextureType.myRenderTarget, aDepth->myTargetTextureType.myDepth);
	}
	else
	{
		DX11Framework::Context->OMSetRenderTargets(1, &myTargetTextureType.myRenderTarget, nullptr);
	}
	DX11Framework::Context->RSSetViewports(1, myViewport);
}

void DX11FullscreenTexture::SetAsResourceOnSlot(uint8_t aSlot)
{
	DX11Framework::Context->PSSetShaderResources(aSlot, 1, &mySRV);
}

void DX11FullscreenTexture::Release()
{
	// myRenderTarget and myDepth share union storage, release only once
	if (myTargetTextureType.myRenderTarget)
	{
		myTargetTextureType.myRenderTarget->Release();
		myTargetTextureType.myRenderTarget = nullptr;
	}
	if (mySRV)
	{
		mySRV->Release();
		mySRV = nullptr;
	}
	if (myTexture)
	{
		myTexture->Release();
		myTexture = nullptr;
	}
	if (myViewport)
	{
		delete myViewport;
		myViewport = nullptr;
	}
}