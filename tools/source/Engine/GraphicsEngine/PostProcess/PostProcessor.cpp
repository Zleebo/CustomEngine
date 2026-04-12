#include <stdafx.h>
#include "PostProcessor.h"
#include <DX11Framework.h>
#include <DX11FullscreenTextureFactory.h>
#include <fstream>

PostProcessor& PostProcessor::Get()
{
	static PostProcessor instance;
	return instance;
}

bool PostProcessor::Init()
{
	auto* device = DX11Framework::Device;
	auto res = DX11Framework::GetResolution();

	myBloomTexture = DX11FullscreenTextureFactory::CreateTexture(res, DXGI_FORMAT_R8G8B8A8_UNORM);

	D3D11_BUFFER_DESC bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufDesc.ByteWidth = sizeof(PostProcessBuffer);
	if (FAILED(device->CreateBuffer(&bufDesc, nullptr, &myConstantBuffer))) return false;

	std::ifstream vsFile(Settings::paths::shaders + "/PostProcessVS.vs.cso", std::ios::binary);
	if (!vsFile.is_open()) return true;
	std::string vsData = { std::istreambuf_iterator<char>(vsFile), std::istreambuf_iterator<char>() };

	if (FAILED(device->CreateVertexShader(vsData.data(), vsData.size(), nullptr, &myFullscreenVS))) return false;

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "UV",       0, DXGI_FORMAT_R32G32_FLOAT,        0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(device->CreateInputLayout(layout, 2, vsData.data(), vsData.size(), &myInputLayout))) return false;

	struct QuadVertex { float x, y, z, w, u, v; };
	QuadVertex quad[6] = {
		{ -1,  1, 0, 1,  0, 0 },
		{  1,  1, 0, 1,  1, 0 },
		{ -1, -1, 0, 1,  0, 1 },
		{  1,  1, 0, 1,  1, 0 },
		{  1, -1, 0, 1,  1, 1 },
		{ -1, -1, 0, 1,  0, 1 },
	};

	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.ByteWidth = sizeof(quad);
	D3D11_SUBRESOURCE_DATA initData = { quad };
	if (FAILED(device->CreateBuffer(&vbDesc, &initData, &myQuadVB))) return false;

	auto loadPS = [&](const std::string& aFile, ID3D11PixelShader** aOut)
	{
		std::ifstream f(Settings::paths::shaders + "/" + aFile, std::ios::binary);
		if (!f.is_open()) return;
		std::string data = { std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>() };
		device->CreatePixelShader(data.data(), data.size(), nullptr, aOut);
	};

	loadPS("TonemapPS.ps.cso",        &myTonemapPS);
	loadPS("BloomExtractPS.ps.cso",   &myBloomExtractPS);
	loadPS("BloomCompositePS.ps.cso", &myBloomCompositePS);

	myInitialized = myFullscreenVS != nullptr && myTonemapPS != nullptr;
	return true;
}

void PostProcessor::DrawFullscreenQuad()
{
	auto* context = DX11Framework::Context;
	UINT stride = sizeof(float) * 6, offset = 0;
	context->IASetVertexBuffers(0, 1, &myQuadVB, &stride, &offset);
	context->IASetInputLayout(myInputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->Draw(6, 0);
}

void PostProcessor::Process(DX11FullscreenTexture& aSource, DX11FullscreenTexture& aTarget)
{
	if (!myInitialized) return;

	auto* context = DX11Framework::Context;

	PostProcessBuffer buf;
	buf.myExposure       = myExposure;
	buf.myBloomThreshold = myBloomThreshold;
	buf.myBloomStrength  = myBloomStrength;
	buf.myBloomEnabled   = myBloomEnabled ? 1 : 0;

	D3D11_MAPPED_SUBRESOURCE msr = {};
	context->Map(myConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
	memcpy(msr.pData, &buf, sizeof(buf));
	context->Unmap(myConstantBuffer, 0);
	context->PSSetConstantBuffers(0, 1, &myConstantBuffer);

	context->VSSetShader(myFullscreenVS, nullptr, 0);

	if (myBloomEnabled && myBloomExtractPS && myBloomCompositePS)
	{
		myBloomTexture.ClearTexture();
		myBloomTexture.SetAsActiveTarget();
		context->PSSetShader(myBloomExtractPS, nullptr, 0);
		aSource.SetAsResourceOnSlot(0);
		DrawFullscreenQuad();

		aTarget.SetAsActiveTarget();
		context->PSSetShader(myBloomCompositePS, nullptr, 0);
		aSource.SetAsResourceOnSlot(0);
		myBloomTexture.SetAsResourceOnSlot(1);
		DrawFullscreenQuad();
	}
	else
	{
		aTarget.SetAsActiveTarget();
		context->PSSetShader(myTonemapPS, nullptr, 0);
		aSource.SetAsResourceOnSlot(0);
		DrawFullscreenQuad();
	}

	ID3D11ShaderResourceView* nullSRV = nullptr;
	context->PSSetShaderResources(0, 1, &nullSRV);
	context->PSSetShaderResources(1, 1, &nullSRV);
}
