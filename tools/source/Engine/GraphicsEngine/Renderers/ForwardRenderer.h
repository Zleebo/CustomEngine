#pragma once
#include <d3d11.h>
#include <iosfwd>
#include <vector>

class ModelInstance;
class Camera;

class ForwardRenderer
{
public:
	ForwardRenderer() = default;
	~ForwardRenderer() = default;
	bool Init();
	void Render(const Camera *camera, const std::vector<ModelInstance*> &someModelInstances);
	void RenderID(const Camera* camera, const std::vector<ModelInstance*>& someModelInstances);

private:
	struct FrameBufferData
	{
		Matrix4x4f to_camera;
		Matrix4x4f to_projection;
	} _frame_buffer_data;

	struct ObjectBufferData
	{
		Matrix4x4f to_world;
		uint32_t id;
		float _pad[3]{};
	} _object_buffer_data;

	struct LightBufferData
	{
		Vector4f dirLightDirection;
		Vector4f dirLightColour;
		Vector4f cameraPosition;
		uint32_t numPointLights;
		float _pad[3]{};
	} myLightBufferData;

	ID3D11Buffer* _frame_buffer = nullptr;
	ID3D11Buffer* _object_buffer = nullptr;
	ID3D11Buffer* myLightBuffer = nullptr;

	ID3D11VertexShader* myIDVertexShader = nullptr;
	ID3D11PixelShader* myIDPixelShader = nullptr;
};

