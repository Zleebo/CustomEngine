#include <stdafx.h>
#include "ParticleRenderer.h"
#include "ParticleEmitter.h"
#include <DX11Framework.h>
#include <Camera/Camera.h>
#include <fstream>
#include <algorithm>

ParticleRenderer& ParticleRenderer::Get()
{
	static ParticleRenderer instance;
	return instance;
}

bool ParticleRenderer::Init()
{
	auto* device = DX11Framework::Device;

	D3D11_BUFFER_DESC bufDesc = {};
	bufDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufDesc.ByteWidth = sizeof(ParticleVertex) * myMaxParticles;
	if (FAILED(device->CreateBuffer(&bufDesc, nullptr, &myVertexBuffer))) return false;

	bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufDesc.ByteWidth = sizeof(FrameBufferData);
	if (FAILED(device->CreateBuffer(&bufDesc, nullptr, &myFrameBuffer))) return false;

	std::ifstream vsFile(Settings::paths::shaders + "/ParticleVS.vs.cso", std::ios::binary);
	if (!vsFile.is_open()) return true;
	std::string vsData = { std::istreambuf_iterator<char>(vsFile), std::istreambuf_iterator<char>() };

	if (FAILED(device->CreateVertexShader(vsData.data(), vsData.size(), nullptr, &myVertexShader))) return false;

	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "PSIZE",    0, DXGI_FORMAT_R32_FLOAT,           0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if (FAILED(device->CreateInputLayout(layout, 3, vsData.data(), vsData.size(), &myInputLayout))) return false;

	std::ifstream psFile(Settings::paths::shaders + "/ParticlePS.ps.cso", std::ios::binary);
	if (!psFile.is_open()) return true;
	std::string psData = { std::istreambuf_iterator<char>(psFile), std::istreambuf_iterator<char>() };
	device->CreatePixelShader(psData.data(), psData.size(), nullptr, &myPixelShader);

	std::ifstream gsFile(Settings::paths::shaders + "/ParticleGS.gs.cso", std::ios::binary);
	if (!gsFile.is_open()) return true;
	std::string gsData = { std::istreambuf_iterator<char>(gsFile), std::istreambuf_iterator<char>() };
	device->CreateGeometryShader(gsData.data(), gsData.size(), nullptr, &myGeometryShader);

	return true;
}

void ParticleRenderer::RegisterEmitter(ParticleEmitter* aEmitter)
{
	myEmitters.push_back(aEmitter);
}

void ParticleRenderer::UnregisterEmitter(ParticleEmitter* aEmitter)
{
	myEmitters.erase(std::remove(myEmitters.begin(), myEmitters.end(), aEmitter), myEmitters.end());
}

void ParticleRenderer::Render(const Camera* aCamera, const std::vector<ParticleEmitter*>& someEmitters)
{
	if (!myVertexShader || !myPixelShader || !myGeometryShader) return;

	auto* context = DX11Framework::Context;

	Matrix4x4f world = aCamera->GetTransform().GetMatrix();
	myFrameData.toCamera     = Matrix4x4f::GetFastInverse(world);
	myFrameData.toProjection = aCamera->get_projection();
	myFrameData.cameraRight  = world.GetRight();
	myFrameData.cameraUp     = world.GetUp();

	D3D11_MAPPED_SUBRESOURCE msr = {};
	context->Map(myFrameBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
	memcpy(msr.pData, &myFrameData, sizeof(FrameBufferData));
	context->Unmap(myFrameBuffer, 0);
	context->VSSetConstantBuffers(0, 1, &myFrameBuffer);
	context->GSSetConstantBuffers(0, 1, &myFrameBuffer);

	std::vector<ParticleVertex> verts;
	verts.reserve(myMaxParticles);

	for (auto* emitter : someEmitters)
	{
		for (const auto& p : emitter->GetParticles())
		{
			if (!p.myAlive) continue;
			ParticleVertex v;
			v.myPosition = p.myPosition;
			v.myColour   = p.myColour;
			v.mySize     = p.mySize;
			verts.push_back(v);
		}
	}

	if (verts.empty()) return;

	context->Map(myVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
	memcpy(msr.pData, verts.data(), sizeof(ParticleVertex) * verts.size());
	context->Unmap(myVertexBuffer, 0);

	UINT stride = sizeof(ParticleVertex), offset = 0;
	context->IASetVertexBuffers(0, 1, &myVertexBuffer, &stride, &offset);
	context->IASetInputLayout(myInputLayout);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	context->VSSetShader(myVertexShader, nullptr, 0);
	context->GSSetShader(myGeometryShader, nullptr, 0);
	context->PSSetShader(myPixelShader, nullptr, 0);

	context->Draw(static_cast<UINT>(verts.size()), 0);

	context->GSSetShader(nullptr, nullptr, 0);
}
