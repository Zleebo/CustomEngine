#include <stdafx.h>

#include "Model.h"

#include <DX11Framework.h>
#include <DDSTextureLoader11.h>
#include <Texture2D.h>

#include <fstream>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <vector>

namespace
{
	struct ObjVertRef
	{
		int pos = -1;
		int uv = -1;
	};

	int ResolveObjIndex(int idx, int count)
	{
		if (idx > 0) return idx - 1;
		if (idx < 0) return count + idx;
		return -1;
	}

	bool ParseObjVertexRef(const std::string& token, ObjVertRef& outRef)
	{
		outRef = {};
		size_t firstSlash = token.find('/');
		if (firstSlash == std::string::npos)
		{
			try { outRef.pos = std::stoi(token); }
			catch (...) { return false; }
			return true;
		}

		try
		{
			outRef.pos = std::stoi(token.substr(0, firstSlash));
		}
		catch (...)
		{
			return false;
		}

		size_t secondSlash = token.find('/', firstSlash + 1);
		if (secondSlash == std::string::npos)
		{
			const std::string uvStr = token.substr(firstSlash + 1);
			if (!uvStr.empty())
			{
				try { outRef.uv = std::stoi(uvStr); }
				catch (...) { outRef.uv = -1; }
			}
			return true;
		}

		const std::string uvStr = token.substr(firstSlash + 1, secondSlash - firstSlash - 1);
		if (!uvStr.empty())
		{
			try { outRef.uv = std::stoi(uvStr); }
			catch (...) { outRef.uv = -1; }
		}

		return true;
	}

	std::string ResolveModelPath(const std::string& path)
	{
		namespace fs = std::filesystem;
		if (path.empty()) return path;

		const fs::path raw(path);
		if (fs::exists(raw))
			return raw.string();

		const fs::path c1 = fs::path(Settings::paths::game_assets) / "Models" / raw;
		if (fs::exists(c1))
			return c1.string();

		const fs::path c2 = fs::path(Settings::paths::game_assets) / raw;
		if (fs::exists(c2))
			return c2.string();

		const fs::path c3 = fs::path(Settings::paths::game_asset_root) / raw;
		if (fs::exists(c3))
			return c3.string();

		return path;
	}

	bool LoadObjMesh(const std::string& objPath, std::vector<Vertex>& outVertices, std::vector<unsigned int>& outIndices)
	{
		std::ifstream in(objPath);
		if (!in.is_open())
			return false;

		struct ObjPos { float x, y, z; };
		struct ObjUv { float u, v; };
		std::vector<ObjPos> positions;
		std::vector<ObjUv> uvs;

		std::string line;
		while (std::getline(in, line))
		{
			if (line.size() < 2) continue;
			if (line.rfind("v ", 0) == 0)
			{
				std::istringstream ss(line.substr(2));
				ObjPos p{};
				if (ss >> p.x >> p.y >> p.z)
					positions.push_back(p);
				continue;
			}
			if (line.rfind("vt ", 0) == 0)
			{
				std::istringstream ss(line.substr(3));
				ObjUv uv{};
				if (ss >> uv.u >> uv.v)
					uvs.push_back(uv);
				continue;
			}
			if (line.rfind("f ", 0) == 0)
			{
				std::istringstream ss(line.substr(2));
				std::vector<ObjVertRef> face;
				std::string token;
				while (ss >> token)
				{
					ObjVertRef r{};
					if (ParseObjVertexRef(token, r))
						face.push_back(r);
				}

				if (face.size() < 3)
					continue;

				for (size_t i = 1; i + 1 < face.size(); ++i)
				{
					const ObjVertRef tri[3] = { face[0], face[i], face[i + 1] };
					for (int c = 0; c < 3; ++c)
					{
						const int pIdx = ResolveObjIndex(tri[c].pos, static_cast<int>(positions.size()));
						if (pIdx < 0 || pIdx >= static_cast<int>(positions.size()))
							continue;

						Vertex v{};
						v.x = positions[pIdx].x;
						v.y = positions[pIdx].y;
						v.z = positions[pIdx].z;
						v.w = 1.0f;
						v.r = 1.0f;
						v.g = 1.0f;
						v.b = 1.0f;
						v.a = 1.0f;
						v.u = 0.0f;
						v.v = 0.0f;

						const int uvIdx = ResolveObjIndex(tri[c].uv, static_cast<int>(uvs.size()));
						if (uvIdx >= 0 && uvIdx < static_cast<int>(uvs.size()))
						{
							v.u = uvs[uvIdx].u;
							v.v = 1.0f - uvs[uvIdx].v;
						}

						outIndices.push_back(static_cast<unsigned int>(outVertices.size()));
						outVertices.push_back(v);
					}
				}
			}
		}

		return !outVertices.empty() && !outIndices.empty();
	}

	void BuildCube(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices)
	{
		vertices = {
			{0.5f, -0.5f, 0.5f, 1, 1, 1, 1, 1, 0, 1},
			{0.5f, 0.5f, 0.5f, 1, 1, 1, 1, 1, 0, 0},
			{-0.5f, 0.5f, 0.5f, 1, 1, 1, 1, 1, 1, 0},
			{-0.5f, -0.5f, 0.5f, 1, 1, 1, 1, 1, 1, 1},

			{-0.5f, -0.5f, 0.5f, 1, 1, 0, 0, 1, 0, 1},
			{-0.5f, 0.5f, 0.5f, 1, 1, 0, 0, 1, 0, 0},
			{-0.5f, 0.5f, -0.5f, 1, 1, 0, 0, 1, 1, 0},
			{-0.5f, -0.5f, -0.5f, 1, 1, 0, 0, 1, 1, 1},

			{-0.5f, -0.5f, -0.5f, 1, 0, 1, 0, 1, 0, 1},
			{-0.5f, 0.5f, -0.5f, 1, 0, 1, 0, 1, 0, 0},
			{0.5f, 0.5f, -0.5f, 1, 0, 1, 0, 1, 1, 0},
			{0.5f, -0.5f, -0.5f, 1, 0, 1, 0, 1, 1, 1},

			{0.5f, -0.5f, -0.5f, 1, 0, 0, 1, 1, 0, 1},
			{0.5f, 0.5f, -0.5f, 1, 0, 0, 1, 1, 0, 0},
			{0.5f, 0.5f, 0.5f, 1, 0, 0, 1, 1, 1, 0},
			{0.5f, -0.5f, 0.5f, 1, 0, 0, 1, 1, 1, 1},

			{0.5f, 0.5f, 0.5f, 1, 1, 1, 0, 1, 0, 1},
			{0.5f, 0.5f, -0.5f, 1, 1, 1, 0, 1, 0, 0},
			{-0.5f, 0.5f, -0.5f, 1, 1, 1, 0, 1, 1, 0},
			{-0.5f, 0.5f, 0.5f, 1, 1, 1, 0, 1, 1, 1},

			{-0.5f, -0.5f, 0.5f, 1, 1, 0, 1, 1, 0, 1},
			{-0.5f, -0.5f, -0.5f, 1, 1, 0, 1, 1, 0, 0},
			{0.5f, -0.5f, -0.5f, 1, 1, 0, 1, 1, 1, 0},
			{0.5f, -0.5f, 0.5f, 1, 1, 0, 1, 1, 1, 1},
		};

		indices = {
			0, 1, 2,   0, 2, 3,
			4, 5, 6,   4, 6, 7,
			8, 9, 10,  8, 10, 11,
			12, 13, 14, 12, 14, 15,
			16, 17, 18, 16, 18, 19,
			20, 21, 22, 20, 22, 23,
		};
	}
}

Model::Model(const char* aPath) : myPath(aPath ? aPath : "")
{
    HRESULT result;

    std::vector<Vertex>       vertices;
    std::vector<unsigned int> indices;

    if (myPath == "Sphere")
    {
        constexpr float PI      = 3.14159265358979f;
        constexpr int   stacks  = 12;
        constexpr int   slices  = 12;

        for (int stack = 0; stack <= stacks; ++stack)
        {
            float phi = PI * stack / stacks;
            for (int slice = 0; slice <= slices; ++slice)
            {
                float theta = 2.0f * PI * slice / slices;
                float x = sinf(phi) * cosf(theta);
                float y = cosf(phi);
                float z = sinf(phi) * sinf(theta);
                float u = (float)slice / slices;
                float v = (float)stack / stacks;
                vertices.push_back({x, y, z, 1.0f,  1.0f, 1.0f, 1.0f, 1.0f,  u, v});
            }
        }

        for (int stack = 0; stack < stacks; ++stack)
        {
            for (int slice = 0; slice < slices; ++slice)
            {
                unsigned int i0 = stack * (slices + 1) + slice;
                unsigned int i1 = i0 + 1;
                unsigned int i2 = i0 + (slices + 1);
                unsigned int i3 = i2 + 1;
                indices.push_back(i0); indices.push_back(i1); indices.push_back(i2);
                indices.push_back(i1); indices.push_back(i3); indices.push_back(i2);
            }
        }
    } else if (myPath != "Cube" && !myPath.empty())
    {
        const std::string resolved = ResolveModelPath(myPath);
        if (!LoadObjMesh(resolved, vertices, indices))
        {
            std::string msg = std::string("[Model] Failed to load mesh '") + myPath + "', using cube fallback\n";
            OutputDebugStringA(msg.c_str());
            BuildCube(vertices, indices);
        }
    } else
    {
        BuildCube(vertices, indices);
    }

    D3D11_BUFFER_DESC vertexBufferDesc{};
    vertexBufferDesc.ByteWidth = (UINT)(vertices.size() * sizeof(Vertex));
    vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexSubresourceData{};
    vertexSubresourceData.pSysMem = vertices.data();

    ID3D11Buffer* vertexBuffer;
    result = DX11Framework::Device->CreateBuffer(&vertexBufferDesc, &vertexSubresourceData, &vertexBuffer);
    if (FAILED(result)) return;

    D3D11_BUFFER_DESC indexBufferDesc{};
    indexBufferDesc.ByteWidth = (UINT)(indices.size() * sizeof(unsigned int));
    indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexSubresourceData{};
    indexSubresourceData.pSysMem = indices.data();

    ID3D11Buffer* indexBuffer;
    result = DX11Framework::Device->CreateBuffer(&indexBufferDesc, &indexSubresourceData, &indexBuffer);
    if (FAILED(result)) return;

    //Start Shader
    std::ifstream vsFile;
    vsFile.open(Settings::paths::shaders + "/VertexShader.vs.cso", std::ios::binary);
    std::string vsData = { std::istreambuf_iterator<char>(vsFile), std::istreambuf_iterator<char>() };
    ID3D11VertexShader* vertexShader;
    result = DX11Framework::Device->CreateVertexShader(vsData.data(), vsData.size(), nullptr, &vertexShader);
    if (FAILED(result)) return;
    vsFile.close();

    std::ifstream psFile;
    psFile.open(Settings::paths::shaders + "/PixelShader.ps.cso", std::ios::binary);
    std::string psData = { std::istreambuf_iterator<char>(psFile), std::istreambuf_iterator<char>() };
    ID3D11PixelShader* pixelShader;
    result = DX11Framework::Device->CreatePixelShader(psData.data(), psData.size(), nullptr, &pixelShader);
    if (FAILED(result)) return;
    psFile.close();
    //End Shader

    //Start Layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "UV",       0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    ID3D11InputLayout* inputLayout;
    result = DX11Framework::Device->CreateInputLayout(layout, sizeof(layout) / sizeof(D3D11_INPUT_ELEMENT_DESC), vsData.data(), vsData.size(), &inputLayout);
    if (FAILED(result)) return;
    //End Layout

    myModelData.num_vertices        = (UINT)vertices.size();
    myModelData.num_indices         = (UINT)indices.size();
    myModelData.stride              = sizeof(Vertex);
    myModelData.offset              = 0;
    myModelData.vertex_buffer       = vertexBuffer;
    myModelData.index_buffer        = indexBuffer;
    myModelData.vertex_shader       = vertexShader;
    myModelData.pixel_shader        = pixelShader;
    myModelData.primitive_topology  = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    myModelData.input_layout        = inputLayout;
}
