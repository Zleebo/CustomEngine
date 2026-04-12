cbuffer FrameBuffer	: register(b0) {
	float4x4 to_camera;
	float4x4 to_projection;
}

cbuffer ObjectBuffer : register(b1) {
	float4x4 to_world;
	int id;
}

struct VertexInput {
	float4 position	:	POSITION;
	float4 color	:	COLOR;
	float2 uv		:	UV;
};

struct VertexToPixel {
	float4 position	:	SV_POSITION;
	float4 color	:	COLOR;
	float2 uv		:	UV;
};

struct PixelOutput {
	float4 color		:	SV_TARGET;
};

Texture2D _instance_texture : register(t0);
SamplerState _default_sampler : register(s0);