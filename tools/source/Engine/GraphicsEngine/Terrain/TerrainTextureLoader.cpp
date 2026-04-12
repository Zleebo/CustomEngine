// No precompiled header — stb_image must not be compiled with PCH
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <d3d11.h>
#include <string>
#include <vector>

ID3D11ShaderResourceView* TerrainLoadTexturePNG(const std::string& path, int slot, bool isNormal, ID3D11Device* device, ID3D11DeviceContext* context)
{
	int width, height, channels;
	unsigned char* img = stbi_load(path.c_str(), &width, &height, &channels, 0);
	if (!img) return nullptr;

	std::vector<unsigned char> imageData;
	if (channels == 3)
	{
		imageData.resize(width * height * 4);
		for (int i = 0; i < width * height; i++)
		{
			imageData[4 * i + 0] = img[3 * i + 0];
			imageData[4 * i + 1] = img[3 * i + 1];
			imageData[4 * i + 2] = img[3 * i + 2];
			imageData[4 * i + 3] = 255;
		}
	}
	else if (channels == 4)
	{
		imageData.assign(img, img + width * height * 4);
	}
	else
	{
		stbi_image_free(img);
		return nullptr;
	}
	stbi_image_free(img);

	DXGI_FORMAT fmt = isNormal ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width            = (UINT)width;
	texDesc.Height           = (UINT)height;
	texDesc.MipLevels        = 1;
	texDesc.ArraySize        = 1;
	texDesc.Format           = fmt;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage            = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem     = imageData.data();
	initData.SysMemPitch = (UINT)(width * 4);

	ID3D11Texture2D* tex = nullptr;
	if (FAILED(device->CreateTexture2D(&texDesc, &initData, &tex)))
		return nullptr;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format                    = fmt;
	srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels       = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	ID3D11ShaderResourceView* srv = nullptr;
	if (FAILED(device->CreateShaderResourceView(tex, &srvDesc, &srv)))
	{
		tex->Release();
		return nullptr;
	}
	tex->Release();

	context->PSSetShaderResources((UINT)slot, 1, &srv);
	// caller owns the SRV lifetime
	return srv;
}
