#pragma once
#include <d3d11.h>
#include <vector>
#include <Math/Vector.h>

class ParticleEmitter;
class Camera;

class ParticleRenderer
{
public:
	static ParticleRenderer& Get();

	bool Init();
	void Render(const Camera* aCamera, const std::vector<ParticleEmitter*>& someEmitters);

	void RegisterEmitter(ParticleEmitter* aEmitter);
	void UnregisterEmitter(ParticleEmitter* aEmitter);

private:
	ParticleRenderer() = default;

	struct ParticleVertex
	{
		Vector3f myPosition;
		Vector4f myColour;
		float mySize;
	};

	struct FrameBufferData
	{
		Matrix4x4f toCamera;
		Matrix4x4f toProjection;
		Vector3f cameraRight;
		float _pad0;
		Vector3f cameraUp;
		float _pad1;
	} myFrameData;

	ID3D11Buffer* myVertexBuffer = nullptr;
	ID3D11Buffer* myFrameBuffer = nullptr;
	ID3D11VertexShader* myVertexShader = nullptr;
	ID3D11PixelShader* myPixelShader = nullptr;
	ID3D11GeometryShader* myGeometryShader = nullptr;
	ID3D11InputLayout* myInputLayout = nullptr;

	std::vector<ParticleEmitter*> myEmitters;
	int myMaxParticles = 10000;
};
