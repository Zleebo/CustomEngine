#include <stdafx.h>
#include "Texture2D.h"

#include <DX11Framework.h>
#include <DDSTextureLoader11.h>
#include <WICTextureLoader11.h>
#include <filesystem>

namespace
{
	std::string ResolveTexturePath(const std::string& path)
	{
		namespace fs = std::filesystem;
		if (path.empty()) return path;

		const fs::path raw(path);
		if (fs::exists(raw))
			return raw.string();

		const fs::path c1 = fs::path(Settings::paths::game_assets) / "Textures" / raw;
		if (fs::exists(c1))
			return c1.string();

		const fs::path c2 = fs::path(Settings::paths::game_assets) / raw;
		if (fs::exists(c2))
			return c2.string();

		return path;
	}
}

std::unordered_map<std::string, ID3D11ShaderResourceView*> DX_Texture2D::myTextureCache;

ID3D11ShaderResourceView* DX_Texture2D::GetTexture(const char *path)
{
	if (path == nullptr || path[0] == '\0')
	{
		return nullptr;
	}

	if (myTextureCache.count(path) > 0)
	{
		return myTextureCache[path];
	}

    std::string asset_path = ResolveTexturePath(path);
    std::wstring wpath(asset_path.begin(), asset_path.end());

    ID3D11ShaderResourceView* SRV = nullptr;
    HRESULT result = DirectX::CreateDDSTextureFromFile(DX11Framework::Device, wpath.c_str(), nullptr, &SRV);
    if (FAILED(result))
    {
		result = DirectX::CreateWICTextureFromFile(DX11Framework::Device, wpath.c_str(), nullptr, &SRV);
		if (FAILED(result))
		{
			std::string msg = std::string("[Texture] Failed to load texture: ") + asset_path + "\n";
			OutputDebugStringA(msg.c_str());
		}
    }

	myTextureCache[path] = SRV;
    return myTextureCache[path];
}
