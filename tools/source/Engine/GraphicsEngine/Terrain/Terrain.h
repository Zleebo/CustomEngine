#pragma once
#include <wrl/client.h>
#include <Math/Matrix4x4.h>
#include <Math/Vector.h>
#include <vector>
#include <string>
#include <cstdint>
using Microsoft::WRL::ComPtr;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Buffer;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;

struct TerrainParams
{
	uint64_t seed        = 12345;
	float    heightScale = 5.0f;
	float    yOffset     = -50.0f;
};

class Terrain
{
public:
	Terrain();
	~Terrain();
	void FlipMatrix();
	void FlipBackMatrix();
	Matrix4x4f& GetTransform() { return myTransform; }
	inline Vector3f GetPos() const { return{ myTransform(4, 1), myTransform(4, 2), myTransform(4, 3) }; }
	bool FlattenGround(float aY = 0.0f);
	void SetPixelShader(const std::string& aPixelShader);
	void SetVertexShader(const std::string& aVertexShader);
	void SetClipHeight(float aClipHeight);
	bool Initialize(ID3D11Device*, const TerrainParams& params = {});
	void Rebuild(const TerrainParams& params);   // regenerates mesh in-place
	void Render(ID3D11DeviceContext*);
	void Render(ID3D11DeviceContext*, ID3D11PixelShader* aPixelShaderOverride);

	// Returns terrain surface Y (world space) at the given world (X, Z)
	float GetHeightAt(float worldX, float worldZ) const;

	int GetIndexCount();

	// Accessors for the physics triangle-mesh collision path
	const float* GetHeightDataPtr()      const { return myHeightData.data(); }
	int          GetTerrainWidthCells()  const { return myTerrainWidth; }
	int          GetTerrainHeightCells() const { return myTerrainHeight; }
	float        GetWorldOffsetX()       const { return myTransform(4, 1); }
	float        GetWorldOffsetY()       const { return myTransform(4, 2); }
	float        GetWorldOffsetZ()       const { return myTransform(4, 3); }
private:
	bool LoadShaders(ID3D11Device*);
	bool InitializeBuffers(ID3D11Device*);
	void RenderBuffers(ID3D11DeviceContext*);

	std::vector<float>   myHeightData;   // local-space Y per vertex [z*(W+1)+x]

	ComPtr<ID3D11Buffer> myVertexBuffer;
	ComPtr<ID3D11Buffer> myIndexBuffer;
	ComPtr<ID3D11VertexShader> myVertexShader;
	ComPtr<ID3D11PixelShader> myPixelShader;
	ComPtr<ID3D11InputLayout> myInputLayout;
	ComPtr<ID3D11Buffer> myObjectBuffer;
	std::string myVertexShaderPath;
	std::string myPixelShaderPath;
	int myTerrainWidth, myTerrainHeight;
	int myVertexCount, myIndexCount;
	ID3D11Device* myDevicePtr;
	Matrix4x4f    myTransform;
	float         myClipHeight = -9999.0f;
	TerrainParams myParams;
	float         myBaseY      = -50.0f;
};
