#pragma once

#include <d3d11.h>
#include <string>

struct Material;

class Model
{
public:
	struct ModelData
	{
		UINT num_vertices;
		UINT num_indices;
		UINT stride;
		UINT offset;
		ID3D11Buffer* vertex_buffer;
		ID3D11Buffer* index_buffer;
		ID3D11VertexShader* vertex_shader;
		ID3D11PixelShader* pixel_shader;
		D3D_PRIMITIVE_TOPOLOGY primitive_topology;
		ID3D11InputLayout* input_layout;
	};

	Model(const char* aPath = "");

	FORCEINLINE const ModelData* GetModelData() const { return &myModelData; }
	FORCEINLINE const char* GetPath() const { return myPath.c_str(); }

public:
	

private:
	ModelData myModelData = {};
	std::string myPath;
};
