#pragma once
#include <d3d11.h>
#include <Math/Vector.h>
#include <string>
#include <vector>

class UIRenderer
{
public:
	static UIRenderer& Get();

	bool Init();
	void BeginFrame();
	void EndFrame();

	void DrawRect(float aX, float aY, float aW, float aH, Vector4f aColour);
	void DrawProgressBar(float aX, float aY, float aW, float aH, float aValue, Vector4f aFillColour = { 0.2f, 0.8f, 0.2f, 1.0f }, Vector4f aBackColour = { 0.2f, 0.2f, 0.2f, 1.0f });
	void DrawImage(float aX, float aY, float aW, float aH, ID3D11ShaderResourceView* aTexture);

private:
	UIRenderer() = default;

	struct UIVertex
	{
		Vector2f myPosition;
		Vector2f myUV;
		Vector4f myColour;
	};

	struct UIQuad
	{
		UIVertex myVerts[6];
		ID3D11ShaderResourceView* myTexture = nullptr;
	};

	void Flush();
	void AddQuad(float aX, float aY, float aW, float aH, Vector4f aColour, ID3D11ShaderResourceView* aTexture = nullptr);
	Vector2f ScreenToNDC(float aX, float aY) const;

	ID3D11Buffer* myVertexBuffer = nullptr;
	ID3D11VertexShader* myVertexShader = nullptr;
	ID3D11PixelShader* myPixelShader = nullptr;
	ID3D11InputLayout* myInputLayout = nullptr;
	ID3D11BlendState* myBlendState = nullptr;
	ID3D11ShaderResourceView* myWhiteTexture = nullptr;

	std::vector<UIQuad> myQuads;
	int myMaxQuads = 1000;
};
