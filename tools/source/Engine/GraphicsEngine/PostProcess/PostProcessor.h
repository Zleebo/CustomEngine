#pragma once
#include <d3d11.h>
#include <GraphicsEngine/GraphicsFrameworks/DX11/DX11FullscreenTexture.h>

class PostProcessor
{
public:
	static PostProcessor& Get();

	bool Init();
	void Process(DX11FullscreenTexture& aSource, DX11FullscreenTexture& aTarget);

	bool myTonemappingEnabled = true;
	bool myBloomEnabled = false;
	float myExposure = 1.0f;
	float myBloomThreshold = 1.0f;
	float myBloomStrength = 0.5f;

private:
	PostProcessor() = default;

	bool myInitialized = false;

	ID3D11VertexShader* myFullscreenVS = nullptr;
	ID3D11PixelShader* myTonemapPS = nullptr;
	ID3D11PixelShader* myBloomExtractPS = nullptr;
	ID3D11PixelShader* myBloomBlurPS = nullptr;
	ID3D11PixelShader* myBloomCompositePS = nullptr;

	DX11FullscreenTexture myBloomTexture;

	struct PostProcessBuffer
	{
		float myExposure;
		float myBloomThreshold;
		float myBloomStrength;
		int myBloomEnabled;
	};

	ID3D11Buffer* myConstantBuffer = nullptr;
	ID3D11Buffer* myQuadVB = nullptr;
	ID3D11InputLayout* myInputLayout = nullptr;

	void DrawFullscreenQuad();
};
