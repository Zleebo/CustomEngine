#include <stdafx.h>
#include "UIRenderer.h"
#include <DX11Framework.h>
#include <fstream>

UIRenderer& UIRenderer::Get()
{
	static UIRenderer instance;
	return instance;
}

bool UIRenderer::Init()
{
	auto* device = DX11Framework::Device;

	D3D11_BUFFER_DESC bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufDesc.ByteWidth = sizeof(UIVertex) * 6 * myMaxQuads;
	if (FAILED(device->CreateBuffer(&bufDesc, nullptr, &myVertexBuffer))) return false;

	std::ifstream vsFile(Settings::paths::shaders + "/UIVS.vs.cso", std::ios::binary);
	if (!vsFile.is_open()) return true;
	std::string vsData = { std::istreambuf_iterator<char>(vsFile), std::istreambuf_iterator<char>() };

	if (FAILED(device->CreateVertexShader(vsData.data(), vsData.size(), nullptr, &myVertexShader))) return false;

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(device->CreateInputLayout(layout, 3, vsData.data(), vsData.size(), &myInputLayout))) return false;

	std::ifstream psFile(Settings::paths::shaders + "/UIPS.ps.cso", std::ios::binary);
	if (!psFile.is_open()) return true;
	std::string psData = { std::istreambuf_iterator<char>(psFile), std::istreambuf_iterator<char>() };
	device->CreatePixelShader(psData.data(), psData.size(), nullptr, &myPixelShader);

	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	device->CreateBlendState(&blendDesc, &myBlendState);

	// 1x1 white texture for coloured quads
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = texDesc.Height = 1;
	texDesc.MipLevels = texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	UINT white = 0xFFFFFFFF;
	D3D11_SUBRESOURCE_DATA initData = { &white, 4, 0 };
	ID3D11Texture2D* whiteTex = nullptr;
	if (SUCCEEDED(device->CreateTexture2D(&texDesc, &initData, &whiteTex)))
	{
		device->CreateShaderResourceView(whiteTex, nullptr, &myWhiteTexture);
		whiteTex->Release();
	}

	return true;
}

Vector2f UIRenderer::ScreenToNDC(float aX, float aY) const
{
	auto res = DX11Framework::GetResolution();
	return
	{
		(aX / res.X) * 2.0f - 1.0f,
		1.0f - (aY / res.Y) * 2.0f
	};
}

void UIRenderer::AddQuad(float aX, float aY, float aW, float aH, Vector4f aColour, ID3D11ShaderResourceView* aTexture)
{
	UIQuad quad;
	quad.myTexture = aTexture ? aTexture : myWhiteTexture;

	Vector2f tl = ScreenToNDC(aX,      aY);
	Vector2f tr = ScreenToNDC(aX + aW, aY);
	Vector2f bl = ScreenToNDC(aX,      aY + aH);
	Vector2f br = ScreenToNDC(aX + aW, aY + aH);

	auto v = [&](Vector2f p, Vector2f uv) -> UIVertex
	{
		return { p, uv, aColour };
	};

	quad.myVerts[0] = v(tl, {0,0});
	quad.myVerts[1] = v(tr, {1,0});
	quad.myVerts[2] = v(bl, {0,1});
	quad.myVerts[3] = v(tr, {1,0});
	quad.myVerts[4] = v(br, {1,1});
	quad.myVerts[5] = v(bl, {0,1});

	myQuads.push_back(quad);
}

void UIRenderer::DrawRect(float aX, float aY, float aW, float aH, Vector4f aColour)
{
	AddQuad(aX, aY, aW, aH, aColour);
}

void UIRenderer::DrawProgressBar(float aX, float aY, float aW, float aH, float aValue, Vector4f aFillColour, Vector4f aBackColour)
{
	AddQuad(aX, aY, aW, aH, aBackColour);
	AddQuad(aX, aY, aW * aValue, aH, aFillColour);
}

void UIRenderer::DrawImage(float aX, float aY, float aW, float aH, ID3D11ShaderResourceView* aTexture)
{
	AddQuad(aX, aY, aW, aH, { 1,1,1,1 }, aTexture);
}

void UIRenderer::BeginFrame()
{
	myQuads.clear();
}

void UIRenderer::EndFrame()
{
	Flush();
}

void UIRenderer::Flush()
{
	if (myQuads.empty() || !myVertexShader || !myPixelShader) return;

	auto* context = DX11Framework::Context;

	float blendFactor[4] = { 0,0,0,0 };
	context->OMSetBlendState(myBlendState, blendFactor, 0xFFFFFFFF);

	context->IASetInputLayout(myInputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->VSSetShader(myVertexShader, nullptr, 0);
	context->PSSetShader(myPixelShader, nullptr, 0);

	UINT stride = sizeof(UIVertex), offset = 0;
	context->IASetVertexBuffers(0, 1, &myVertexBuffer, &stride, &offset);

	for (const auto& quad : myQuads)
	{
		D3D11_MAPPED_SUBRESOURCE msr = {};
		context->Map(myVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
		memcpy(msr.pData, quad.myVerts, sizeof(UIVertex) * 6);
		context->Unmap(myVertexBuffer, 0);

		context->PSSetShaderResources(0, 1, &quad.myTexture);
		context->Draw(6, 0);
	}

	context->OMSetBlendState(nullptr, blendFactor, 0xFFFFFFFF);
}
