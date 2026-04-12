#pragma once

#include <d3d11.h>

#include <string>
#include <unordered_map>

class DX_Material
{
public:
	static ID3D11ShaderResourceView *GetTexture(const char* path);

private:
	static std::unordered_map<std::string, ID3D11ShaderResourceView*> myTextureCache;
	//ID3D11ShaderResourceView* texture;
	//std::wstring texture_name;
};