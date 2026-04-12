#pragma once
#include "DX11FullscreenTexture.h"
#include <d3d11.h>

class DX11FullscreenTextureFactory
{
public:

	static DX11FullscreenTexture CreateTexture(Vector2ui aSize, DXGI_FORMAT aFormat);
	static DX11FullscreenTexture CreateTexture(ID3D11Texture2D* aTextureTemplate);
	static DX11FullscreenTexture CreateDepth(Vector2ui aSize, DXGI_FORMAT aFormat);
};