#include <stdafx.h>
#include "Texture2D.h"

#include <DX11Framework.h>
#include <DDSTextureLoader11.h>

std::unordered_map<std::string, ID3D11ShaderResourceView*> DX_Texture2D::myTextureCache;

ID3D11ShaderResourceView* DX_Texture2D::GetTexture(const char *path)
{
	if (myTextureCache.count(path) > 0)
	{
		return myTextureCache[path];
	}

    std::string asset_path = path;
    std::wstring wpath = std::wstring(asset_path.begin(), asset_path.end());

    ID3D11ShaderResourceView* SRV;
    HRESULT result = DirectX::CreateDDSTextureFromFile(DX11Framework::Device, wpath.c_str(), nullptr, &SRV);
    if (FAILED(result))
    {
        //return false;
    }

	myTextureCache[path] = SRV;
    return myTextureCache[path];
}