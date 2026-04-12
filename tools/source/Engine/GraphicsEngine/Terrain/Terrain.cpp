#include <stdafx.h>
#include "Terrain.h"
#include <d3d11.h>
#include <fstream>
#include <random>

#ifndef TERRAIN_VERTEX_DEFINED
#define TERRAIN_VERTEX_DEFINED
struct TerrainVertex
{
	Vector3f pos;
	Vector3f normal;
	Vector2f uv;
	Vector3f tangent;
	Vector3f biNormal;
};
#include "uppgift05_helper.h"
#endif

struct ObjectBufferData
{
	Matrix4x4f modelToWorldMatrix;
	float clipHeight[4];
};

Terrain::Terrain()
{
	myPixelShaderPath = "";
	myVertexShaderPath = "";
}
Terrain::~Terrain() = default;

void Terrain::FlipMatrix()
{
	myTransform(2, 2) *= -1.f;
	myTransform(4, 2) += 0.2f;
}

void Terrain::FlipBackMatrix()
{
	myTransform(2, 2) = 1;
	myTransform(4, 2) = myBaseY;
}

bool Terrain::FlattenGround(float aY)
{
	myTransform(2, 2) *= 0.0f;

	myTransform(4, 1) = -(float)myTerrainWidth  * 0.5f;
	myTransform(4, 2) = aY;
	myTransform(4, 3) = -(float)myTerrainHeight * 0.5f;

	myVertexCount = (myTerrainWidth + 1) * (myTerrainHeight + 1);
	myIndexCount = myVertexCount;
	std::vector<TerrainVertex> vertices(myVertexCount);
	std::vector<Vector3f> myTPerV;
	int index = 0;
	std::vector<float> Noise;
	unsigned int resolution = 16;
	for (size_t i = 0; i < resolution * resolution; i++)
	{
		Noise.push_back(RandomFloat());
	}

	float factor = 0.55f;
	for (size_t i = 0; i < 4; i++)
	{
		AddNoise(Noise, pow(factor, i) * 5);
		Noise = Upsample2X(Noise, resolution);
		resolution *= 2;
	}
	std::vector<std::vector<TerrainVertex>> v;
	v.resize(myTerrainHeight + 1, std::vector<TerrainVertex>(myTerrainWidth + 1));
	int vertPos = 0;
	for (size_t z = 0; z <= myTerrainWidth; z++)
	{
		for (size_t x = 0; x <= myTerrainHeight; x++)
		{
			TerrainVertex a;
			a.pos = Vector3f((float)x, 0.11f, (float)z);
			v[z][x] = a;
			vertices[vertPos] = a;
			vertPos++;
		}
	}
	index = 0;
	for (size_t z = 0; z <= myTerrainWidth; z++)
	{
		for (size_t x = 0; x <= myTerrainHeight; x++)
		{
			if (z != 0 && x != 0 && z != (size_t)myTerrainHeight && x != (size_t)myTerrainWidth)
			{
				v[z][x].normal = (v[z + 1][x].pos - v[z - 1][x].pos).Cross(v[z][x + 1].pos - v[z][x - 1].pos);
			}
			vertices[index].normal = v[z][x].normal.GetNormalized();
			vertices[index].uv = Vector2f((float)z / (float)myTerrainWidth, (float)x / (float)myTerrainHeight);
			index++;
		}
	}
	myIndexCount = myTerrainHeight * myTerrainWidth * 6;
	std::vector<unsigned int> triangles(myIndexCount);
	int vert = 0;
	int tris = 0;
	for (size_t z = 0; z < (size_t)myTerrainWidth; z++)
	{
		for (size_t x = 0; x < (size_t)myTerrainHeight; x++)
		{
			triangles[tris + 0] = vert + 0;
			triangles[tris + 1] = vert + myTerrainHeight + 1;
			triangles[tris + 2] = vert + 1;
			triangles[tris + 3] = vert + 1;
			triangles[tris + 4] = vert + myTerrainHeight + 1;
			triangles[tris + 5] = vert + myTerrainHeight + 2;
			vert++;
			tris += 6;
		}
		vert++;
	}
	int vertIndex = 0;
	for (size_t z = 0; z < (size_t)myTerrainWidth; z++)
	{
		for (size_t x = 0; x < (size_t)myTerrainHeight; x++)
		{
			Vector3f c1 = vertices[vertIndex].normal.Cross(Vector3f(0.0f, 0.0f, 1.0f));
			Vector3f c2 = vertices[vertIndex].normal.Cross(Vector3f(0.0f, 1.0f, 0.0f));
			if (c1.Length() > c2.Length())
				vertices[vertIndex].tangent = c1;
			else
				vertices[vertIndex].tangent = c2;
			vertices[vertIndex].tangent = vertices[vertIndex].tangent.GetNormalized();
			vertices[vertIndex].biNormal = vertices[vertIndex].normal.Cross(vertices[vertIndex].tangent).GetNormalized();
			vertIndex++;
		}
	}

	HRESULT result;
	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.ByteWidth = sizeof(TerrainVertex) * myVertexCount;
	vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vertexData = { 0 };
	vertexData.pSysMem = vertices.data();
	result = myDevicePtr->CreateBuffer(&vertexBufferDesc, &vertexData, myVertexBuffer.GetAddressOf());
	if (FAILED(result)) return false;

	D3D11_BUFFER_DESC indexBufferDesc = {};
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(unsigned int) * myIndexCount;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA indexData = { 0 };
	indexData.pSysMem = triangles.data();
	result = myDevicePtr->CreateBuffer(&indexBufferDesc, &indexData, &myIndexBuffer);
	if (FAILED(result)) return false;

	D3D11_BUFFER_DESC bufferDescription = { 0 };
	bufferDescription.Usage = D3D11_USAGE_DYNAMIC;
	bufferDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferDescription.ByteWidth = sizeof(ObjectBufferData);
	result = myDevicePtr->CreateBuffer(&bufferDescription, nullptr, &myObjectBuffer);
	if (FAILED(result)) return false;

	return true;
}

void Terrain::SetPixelShader(const std::string& aPixelShader)
{
	myPixelShaderPath = aPixelShader;
	std::ifstream psFile;
	HRESULT result;
	psFile.open(aPixelShader, std::ios::binary);
	std::string psData = { std::istreambuf_iterator<char>(psFile), std::istreambuf_iterator<char>() };
	result = myDevicePtr->CreatePixelShader(psData.data(), psData.size(), nullptr, myPixelShader.GetAddressOf());
	psFile.close();
}

void Terrain::SetVertexShader(const std::string& aVertexShader)
{
	myVertexShaderPath = aVertexShader;
	HRESULT result;
	std::ifstream vsFile;
	vsFile.open(aVertexShader, std::ios::binary);
	std::string vsData = { std::istreambuf_iterator<char>(vsFile), std::istreambuf_iterator<char>() };
	vsFile.close();
	result = myDevicePtr->CreateVertexShader(vsData.data(), vsData.size(), nullptr, myVertexShader.GetAddressOf());
	if (FAILED(result)) return;

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	unsigned int numElements = sizeof(layout) / sizeof(D3D11_INPUT_ELEMENT_DESC);
	myDevicePtr->CreateInputLayout(layout, numElements, vsData.data(), vsData.size(), myInputLayout.GetAddressOf());
}

bool Terrain::Initialize(ID3D11Device* device, const TerrainParams& params)
{
	myDevicePtr     = device;
	myTerrainWidth  = 200;
	myTerrainHeight = 200;
	myParams        = params;
	myBaseY         = params.yOffset;
	InitializeBuffers(device);
	LoadShaders(device);
	return true;
}

void Terrain::Rebuild(const TerrainParams& params)
{
	myParams = params;
	myBaseY  = params.yOffset;
	myTransform(4, 1) = -(float)myTerrainWidth  * 0.5f;
	myTransform(4, 2) = params.yOffset;
	myTransform(4, 3) = -(float)myTerrainHeight * 0.5f;

	// Regenerate height data with new params
	pcg32_random_t rng;
	rng.state = params.seed;
	rng.inc   = (params.seed << 1u) | 1u;

	unsigned int resolution = 16;
	std::vector<float> Noise;
	Noise.reserve(resolution * resolution);
	for (size_t i = 0; i < resolution * resolution; i++)
		Noise.push_back(RandomFloat(rng));

	const float factor = 0.55f;
	for (size_t i = 0; i < 4; i++)
	{
		AddNoise(Noise, powf(factor, (float)i) * params.heightScale, rng);
		Noise = Upsample2X(Noise, resolution);
		resolution *= 2;
	}

	// Rebuild vertex data
	const int totalVerts = (myTerrainWidth + 1) * (myTerrainHeight + 1);
	auto vertices = new TerrainVertex[totalVerts];
	std::vector<std::vector<TerrainVertex>> v;
	v.resize(myTerrainHeight + 1, std::vector<TerrainVertex>(myTerrainWidth + 1));

	int vertPos = 0;
	for (size_t z = 0; z <= (size_t)myTerrainWidth; z++)
	{
		for (size_t x = 0; x <= (size_t)myTerrainHeight; x++)
		{
			size_t idx = x + z * resolution;
			TerrainVertex a;
			a.pos = Vector3f((float)x, Noise[idx], (float)z);
			v[z][x] = a;
			vertices[vertPos++] = a;
		}
	}
	// Capture height data for collision
	myHeightData.resize((myTerrainWidth + 1) * (myTerrainHeight + 1));
	for (size_t z = 0; z <= (size_t)myTerrainWidth; z++)
		for (size_t x = 0; x <= (size_t)myTerrainHeight; x++)
			myHeightData[z * (myTerrainWidth + 1) + x] = v[z][x].pos.Y;

	int index = 0;
	for (size_t z = 0; z <= (size_t)myTerrainWidth; z++)
	{
		for (size_t x = 0; x <= (size_t)myTerrainHeight; x++)
		{
			if (z != 0 && x != 0 && z != (size_t)myTerrainHeight && x != (size_t)myTerrainWidth)
				v[z][x].normal = (v[z+1][x].pos - v[z-1][x].pos).Cross(v[z][x+1].pos - v[z][x-1].pos);
			vertices[index].normal = v[z][x].normal.GetNormalized();
			vertices[index].uv     = Vector2f((float)z / (float)myTerrainWidth, (float)x / (float)myTerrainHeight);
			index++;
		}
	}
	int vertIndex = 0;
	for (size_t z = 0; z < (size_t)myTerrainWidth; z++)
	{
		for (size_t x = 0; x < (size_t)myTerrainHeight; x++)
		{
			Vector3f c1 = vertices[vertIndex].normal.Cross(Vector3f(0.f, 0.f, 1.f));
			Vector3f c2 = vertices[vertIndex].normal.Cross(Vector3f(0.f, 1.f, 0.f));
			vertices[vertIndex].tangent   = (c1.Length() > c2.Length() ? c1 : c2).GetNormalized();
			vertices[vertIndex].biNormal  = vertices[vertIndex].normal.Cross(vertices[vertIndex].tangent).GetNormalized();
			vertIndex++;
		}
	}

	// Recreate vertex buffer with new data
	myVertexBuffer.Reset();
	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.ByteWidth  = sizeof(TerrainVertex) * totalVerts;
	vbDesc.Usage      = D3D11_USAGE_IMMUTABLE;
	vbDesc.BindFlags  = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem    = vertices;
	myDevicePtr->CreateBuffer(&vbDesc, &vbData, myVertexBuffer.GetAddressOf());

	delete[] vertices;
}

void Terrain::Render(ID3D11DeviceContext* context)
{
	Render(context, nullptr);
}

void Terrain::Render(ID3D11DeviceContext* context, ID3D11PixelShader* aPixelShaderOverride)
{
	RenderBuffers(context);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->IASetInputLayout(myInputLayout.Get());
	unsigned int stride = sizeof(TerrainVertex);
	unsigned int offset = 0;
	context->IASetVertexBuffers(0, 1, myVertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(myIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	context->VSSetShader(myVertexShader.Get(), nullptr, 0);
	context->PSSetShader(aPixelShaderOverride ? aPixelShaderOverride : myPixelShader.Get(), nullptr, 0);
	context->DrawIndexed(myIndexCount, 0, 0);
}

int Terrain::GetIndexCount()
{
	return myIndexCount;
}

bool Terrain::LoadShaders(ID3D11Device* device)
{
	if (myVertexShaderPath.empty()) return true;

	std::string vsData;
	HRESULT result;
	{
		std::ifstream vsFile;
		vsFile.open(myVertexShaderPath, std::ios::binary);
		vsData = { std::istreambuf_iterator<char>(vsFile), std::istreambuf_iterator<char>() };
		result = device->CreateVertexShader(vsData.data(), vsData.size(), nullptr, &myVertexShader);
		if (FAILED(result)) return false;
		vsFile.close();

		if (!myPixelShaderPath.empty())
		{
			std::ifstream psFile;
			psFile.open(myPixelShaderPath, std::ios::binary);
			std::string psData = { std::istreambuf_iterator<char>(psFile), std::istreambuf_iterator<char>() };
			result = device->CreatePixelShader(psData.data(), psData.size(), nullptr, &myPixelShader);
			if (FAILED(result)) return false;
			psFile.close();
		}
	}
	{
		D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		unsigned int numElements = sizeof(layout) / sizeof(D3D11_INPUT_ELEMENT_DESC);
		result = device->CreateInputLayout(layout, numElements, vsData.data(), vsData.size(), &myInputLayout);
		if (FAILED(result)) return false;
	}
	return true;
}

bool Terrain::InitializeBuffers(ID3D11Device* device)
{
	myTransform(4, 1) = -(float)myTerrainWidth  * 0.5f;
	myTransform(4, 2) = myParams.yOffset;
	myTransform(4, 3) = -(float)myTerrainHeight * 0.5f;
	myBaseY           = myParams.yOffset;

	myVertexCount = (myTerrainWidth + 1) * (myTerrainHeight + 1);
	myIndexCount = myVertexCount;
	std::vector<TerrainVertex> vertices(myVertexCount);

	// Seeded noise generation
	pcg32_random_t rng;
	rng.state = myParams.seed;
	rng.inc   = (myParams.seed << 1u) | 1u;

	std::vector<float> Noise;
	unsigned int resolution = 16;
	for (size_t i = 0; i < resolution * resolution; i++)
		Noise.push_back(RandomFloat(rng));

	const float factor = 0.55f;
	for (size_t i = 0; i < 4; i++)
	{
		AddNoise(Noise, powf(factor, (float)i) * myParams.heightScale, rng);
		Noise = Upsample2X(Noise, resolution);
		resolution *= 2;
	}
	std::vector<std::vector<TerrainVertex>> v;
	v.resize(myTerrainHeight + 1, std::vector<TerrainVertex>(myTerrainWidth + 1));
	int vertPos = 0;
	for (size_t z = 0; z <= (size_t)myTerrainWidth; z++)
	{
		for (size_t x = 0; x <= (size_t)myTerrainHeight; x++)
		{
			size_t idx = x + z * resolution;
			TerrainVertex a;
			a.pos = Vector3f((float)x, Noise[idx], (float)z);
			v[z][x] = a;
			vertices[vertPos] = a;
			vertPos++;
		}
	}
	// Store height data for collision queries
	myHeightData.resize((myTerrainWidth + 1) * (myTerrainHeight + 1));
	for (size_t z = 0; z <= (size_t)myTerrainWidth; z++)
		for (size_t x = 0; x <= (size_t)myTerrainHeight; x++)
			myHeightData[z * (myTerrainWidth + 1) + x] = v[z][x].pos.Y;

	int index = 0;
	for (size_t z = 0; z <= (size_t)myTerrainWidth; z++)
	{
		for (size_t x = 0; x <= (size_t)myTerrainHeight; x++)
		{
			if (z != 0 && x != 0 && z != (size_t)myTerrainHeight && x != (size_t)myTerrainWidth)
			{
				v[z][x].normal = (v[z + 1][x].pos - v[z - 1][x].pos).Cross(v[z][x + 1].pos - v[z][x - 1].pos);
			}
			vertices[index].normal = v[z][x].normal.GetNormalized();
			vertices[index].uv = Vector2f((float)z / (float)myTerrainWidth, (float)x / (float)myTerrainHeight);
			index++;
		}
	}
	myIndexCount = myTerrainHeight * myTerrainWidth * 6;
	std::vector<unsigned int> triangles(myIndexCount);
	int vert = 0;
	int tris = 0;
	for (size_t z = 0; z < (size_t)myTerrainWidth; z++)
	{
		for (size_t x = 0; x < (size_t)myTerrainHeight; x++)
		{
			triangles[tris + 0] = vert + 0;
			triangles[tris + 1] = vert + myTerrainHeight + 1;
			triangles[tris + 2] = vert + 1;
			triangles[tris + 3] = vert + 1;
			triangles[tris + 4] = vert + myTerrainHeight + 1;
			triangles[tris + 5] = vert + myTerrainHeight + 2;
			vert++;
			tris += 6;
		}
		vert++;
	}
	int vertIndex = 0;
	for (size_t z = 0; z < (size_t)myTerrainWidth; z++)
	{
		for (size_t x = 0; x < (size_t)myTerrainHeight; x++)
		{
			Vector3f c1 = vertices[vertIndex].normal.Cross(Vector3f(0.0f, 0.0f, 1.0f));
			Vector3f c2 = vertices[vertIndex].normal.Cross(Vector3f(0.0f, 1.0f, 0.0f));
			if (c1.Length() > c2.Length())
				vertices[vertIndex].tangent = c1;
			else
				vertices[vertIndex].tangent = c2;
			vertices[vertIndex].tangent = vertices[vertIndex].tangent.GetNormalized();
			vertices[vertIndex].biNormal = vertices[vertIndex].normal.Cross(vertices[vertIndex].tangent).GetNormalized();
			vertIndex++;
		}
	}

	HRESULT result;
	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.ByteWidth = sizeof(TerrainVertex) * myVertexCount;
	vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vertexData = { 0 };
	vertexData.pSysMem = vertices.data();
	result = device->CreateBuffer(&vertexBufferDesc, &vertexData, myVertexBuffer.GetAddressOf());
	if (FAILED(result)) return false;

	D3D11_BUFFER_DESC indexBufferDesc = {};
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(unsigned int) * myIndexCount;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA indexData = { 0 };
	indexData.pSysMem = triangles.data();
	result = device->CreateBuffer(&indexBufferDesc, &indexData, &myIndexBuffer);
	if (FAILED(result)) return false;

	D3D11_BUFFER_DESC bufferDescription = { 0 };
	bufferDescription.Usage = D3D11_USAGE_DYNAMIC;
	bufferDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferDescription.ByteWidth = sizeof(ObjectBufferData);
	result = device->CreateBuffer(&bufferDescription, nullptr, &myObjectBuffer);
	if (FAILED(result)) return false;

	return true;
}

float Terrain::GetHeightAt(float worldX, float worldZ) const
{
	if (myHeightData.empty()) return myBaseY;

	// To local terrain space
	float lx = worldX - myTransform(4, 1);
	float lz = worldZ - myTransform(4, 3);

	// Clamp to terrain bounds
	lx = lx < 0.f ? 0.f : (lx > (float)myTerrainWidth  ? (float)myTerrainWidth  : lx);
	lz = lz < 0.f ? 0.f : (lz > (float)myTerrainHeight ? (float)myTerrainHeight : lz);

	int ix = (int)lx;
	int iz = (int)lz;
	if (ix >= myTerrainWidth)  ix = myTerrainWidth  - 1;
	if (iz >= myTerrainHeight) iz = myTerrainHeight - 1;

	float fx = lx - (float)ix;
	float fz = lz - (float)iz;

	const int W = myTerrainWidth + 1;
	float h00 = myHeightData[ iz      * W + ix    ];
	float h10 = myHeightData[ iz      * W + ix + 1];
	float h01 = myHeightData[(iz + 1) * W + ix    ];
	float h11 = myHeightData[(iz + 1) * W + ix + 1];

	float h = h00 * (1.f - fx) * (1.f - fz)
	        + h10 * fx          * (1.f - fz)
	        + h01 * (1.f - fx) * fz
	        + h11 * fx          * fz;

	return h + myTransform(4, 2); // add world Y offset
}

void Terrain::SetClipHeight(float aClipHeight)
{
	myClipHeight = aClipHeight;
}

void Terrain::RenderBuffers(ID3D11DeviceContext* context)
{
	ObjectBufferData objectBufferData = {};
	objectBufferData.modelToWorldMatrix = myTransform;
	objectBufferData.clipHeight[0] = myClipHeight;
	objectBufferData.clipHeight[1] = myBaseY;
	D3D11_MAPPED_SUBRESOURCE mappedBuffer = {};
	context->Map(myObjectBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedBuffer);
	memcpy(mappedBuffer.pData, &objectBufferData, sizeof(ObjectBufferData));
	context->Unmap(myObjectBuffer.Get(), 0);
	context->VSSetConstantBuffers(1, 1, myObjectBuffer.GetAddressOf());
	context->PSSetConstantBuffers(1, 1, myObjectBuffer.GetAddressOf());
}
