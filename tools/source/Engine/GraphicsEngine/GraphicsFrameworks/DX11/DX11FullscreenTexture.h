#pragma once

#include <Math/Vector.h>

class DX11FullscreenTexture
{
public:
	DX11FullscreenTexture();
	~DX11FullscreenTexture();

	void ClearTexture(const Vector4f &aClearColor = { 0.0f,0.0f,0.0f,0.0f });
	void ClearTexture(const std::array<float, 4> &aClearColor);
	void ClearDepth(float aClearDepthValue = 1.0f, uint8_t aClearStencilValue = 0);
	void SetAsActiveTarget(DX11FullscreenTexture* aDepth = nullptr);
	void SetAsResourceOnSlot(uint8_t aSlot);
	
	struct ID3D11ShaderResourceView *GetSRV() const { return mySRV; }
	void Release();

	struct ID3D11Texture2D* GetTexture() { return myTexture; }

	union TargetTextureType
	{
		struct ID3D11RenderTargetView* myRenderTarget;
		struct ID3D11DepthStencilView* myDepth;
	} myTargetTextureType;
private:
	friend class DX11FullscreenTextureFactory;

	struct ID3D11Texture2D* myTexture;
	struct ID3D11ShaderResourceView* mySRV;
	struct D3D11_VIEWPORT* myViewport;
};