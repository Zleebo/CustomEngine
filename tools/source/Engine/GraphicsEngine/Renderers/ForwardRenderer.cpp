#include <stdafx.h>
#include <ForwardRenderer.h>

#include <vector>
#include <fstream>

#include <DX11Framework.h>
#include <Texture2D.h>

#include "ModelInstance.h"
#include "Model.h"
#include "Camera/Camera.h"
#include "GraphicsEngine/Lights/LightManager.h"

bool ForwardRenderer::Init()
{
	HRESULT result = S_FALSE;

	ID3D11DeviceContext* Context = DX11Framework::Context;
	ID3D11Device* Device = DX11Framework::Device;

	D3D11_BUFFER_DESC bufferDescription = { 0 };
	bufferDescription.Usage = D3D11_USAGE_DYNAMIC;
	bufferDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	bufferDescription.ByteWidth = sizeof(FrameBufferData);
	result = Device->CreateBuffer(&bufferDescription, nullptr, &_frame_buffer);
	if (FAILED(result))
	{
		return false;
	}

	bufferDescription.ByteWidth = sizeof(ObjectBufferData);
	result = Device->CreateBuffer(&bufferDescription, nullptr, &_object_buffer);
	if (FAILED(result))
	{
		return false;
	}

	bufferDescription.ByteWidth = sizeof(LightBufferData);
	result = Device->CreateBuffer(&bufferDescription, nullptr, &myLightBuffer);
	if (FAILED(result))
	{
		return false;
	}

	std::ifstream vsFile;
	vsFile.open(Settings::paths::shaders + "/VertexIDShader.vs.cso", std::ios::binary);
	std::string vsData = { std::istreambuf_iterator<char>(vsFile), std::istreambuf_iterator<char>() };

	result = DX11Framework::Device->CreateVertexShader(vsData.data(), vsData.size(), nullptr, &myIDVertexShader);
	vsFile.close();

	std::ifstream psFile;
	psFile.open(Settings::paths::shaders + "/PixelIDShader.ps.cso", std::ios::binary);
	std::string psData = { std::istreambuf_iterator<char>(psFile), std::istreambuf_iterator<char>() };

	DX11Framework::Device->CreatePixelShader(psData.data(), psData.size(), nullptr, &myIDPixelShader);
	psFile.close();

	return true;
}

void ForwardRenderer::RenderID(const Camera* camera, const std::vector<ModelInstance*>& someModelInstances)
{
	HRESULT result;
	D3D11_MAPPED_SUBRESOURCE buffer_data;

	auto context = DX11Framework::Context;

	_frame_buffer_data.to_camera = Matrix4x4f::GetFastInverse(camera->GetTransform().GetMatrix());
	_frame_buffer_data.to_projection = camera->get_projection();

	ZeroMemory(&buffer_data, sizeof(D3D11_MAPPED_SUBRESOURCE));

	result = context->Map(_frame_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &buffer_data);
	if (FAILED(result))
	{
		OutputDebugStringA("[ForwardRenderer] RenderID: Map frame buffer failed\n");
		return;
	}

	memcpy(buffer_data.pData, &_frame_buffer_data, sizeof(FrameBufferData));
	context->Unmap(_frame_buffer, 0);
	context->VSSetConstantBuffers(0, 1, &_frame_buffer);

	for (ModelInstance* model_instance : someModelInstances)
	{
		Model* model = model_instance->GetModel();
		const Model::ModelData* model_data = model->GetModelData();

		_object_buffer_data.to_world = model_instance->GetTransform().GetMatrix();
		_object_buffer_data.id = model_instance->GetID();

		ZeroMemory(&buffer_data, sizeof(D3D11_MAPPED_SUBRESOURCE));
		result = context->Map(_object_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &buffer_data);
		if (FAILED(result))
		{
			// Boom!
		}

		memcpy(buffer_data.pData, &_object_buffer_data, sizeof(ObjectBufferData));
		context->Unmap(_object_buffer, 0);

		context->IASetPrimitiveTopology(model_data->primitive_topology);
		context->IASetInputLayout(model_data->input_layout);

		context->IASetVertexBuffers(0, 1, &model_data->vertex_buffer, &model_data->stride, &model_data->offset);
		context->IASetIndexBuffer(model_data->index_buffer, DXGI_FORMAT_R32_UINT, 0);

		context->VSSetConstantBuffers(1, 1, &_object_buffer);
		context->PSSetConstantBuffers(1, 1, &_object_buffer);

		context->VSSetShader(myIDVertexShader, nullptr, 0);
		context->PSSetShader(myIDPixelShader, nullptr, 0);

		auto t = DX_Texture2D::GetTexture(model_instance->GetTexture());
		context->PSSetShaderResources(0, 1, &t);

		context->DrawIndexed(model_data->num_indices, 0, 0);
	}
}

void ForwardRenderer::Render(const Camera* camera, const std::vector<ModelInstance*> &someModelInstances)
{
	HRESULT result;
	D3D11_MAPPED_SUBRESOURCE buffer_data;

	auto context = DX11Framework::Context;

	_frame_buffer_data.to_camera = Matrix4x4f::GetFastInverse(camera->GetTransform().GetMatrix());
	_frame_buffer_data.to_projection = camera->get_projection();

	ZeroMemory(&buffer_data, sizeof(D3D11_MAPPED_SUBRESOURCE));
	result = context->Map(_frame_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &buffer_data);
	if (FAILED(result)) { return; }
	memcpy(buffer_data.pData, &_frame_buffer_data, sizeof(FrameBufferData));
	context->Unmap(_frame_buffer, 0);
	context->VSSetConstantBuffers(0, 1, &_frame_buffer);

	const auto& lights = LightManager::Get();
	myLightBufferData.dirLightDirection = lights.GetDirectionalLight().myDirection;
	myLightBufferData.dirLightColour    = lights.GetDirectionalLight().myColour;
	myLightBufferData.cameraPosition    = { camera->GetTransform().GetPosition().X, camera->GetTransform().GetPosition().Y, camera->GetTransform().GetPosition().Z, 1.0f };
	myLightBufferData.numPointLights    = static_cast<uint32_t>(lights.GetPointLights().size());

	if (!myLightBuffer) return;
	ZeroMemory(&buffer_data, sizeof(D3D11_MAPPED_SUBRESOURCE));
	result = context->Map(myLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &buffer_data);
	if (FAILED(result)) { return; }
	memcpy(buffer_data.pData, &myLightBufferData, sizeof(LightBufferData));
	context->Unmap(myLightBuffer, 0);
	context->PSSetConstantBuffers(2, 1, &myLightBuffer);

	for (ModelInstance* model_instance : someModelInstances)
	{
		Model* model = model_instance->GetModel();
		const Model::ModelData *model_data = model->GetModelData();

		_object_buffer_data.to_world = model_instance->GetTransform().GetMatrix();
		_object_buffer_data.id = model_instance->GetID();
		
		ZeroMemory(&buffer_data, sizeof(D3D11_MAPPED_SUBRESOURCE));
		result = context->Map(_object_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &buffer_data);
		if (FAILED(result))
		{
			OutputDebugStringA("[ForwardRenderer] Render: Map object buffer failed\n");
			continue;
		}

		memcpy(buffer_data.pData, &_object_buffer_data, sizeof(ObjectBufferData));
		context->Unmap(_object_buffer, 0);

		context->IASetPrimitiveTopology(model_data->primitive_topology);
		context->IASetInputLayout(model_data->input_layout);

		context->IASetVertexBuffers(0, 1, &model_data->vertex_buffer, &model_data->stride, &model_data->offset);
		context->IASetIndexBuffer(model_data->index_buffer, DXGI_FORMAT_R32_UINT, 0);

		context->VSSetConstantBuffers(1, 1, &_object_buffer);
		context->PSSetConstantBuffers(1, 1, &_object_buffer);

		context->VSSetShader(model_data->vertex_shader, nullptr, 0);
		context->PSSetShader(model_data->pixel_shader, nullptr, 0);

		auto t = DX_Texture2D::GetTexture(model_instance->GetTexture());
		context->PSSetShaderResources(0, 1, &t);

		context->DrawIndexed(model_data->num_indices, 0, 0);
	}
}
