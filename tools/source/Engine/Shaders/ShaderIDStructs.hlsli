cbuffer FrameBuffer	: register(b0) {
	float4x4 to_camera;
	float4x4 to_projection;
}

cbuffer ObjectBuffer : register(b1) {
	float4x4 to_world;
	int id;
}

struct VertexIDInput {
	float4 position	:	POSITION;
	float4 color	:	COLOR;
	float2 uv		:	UV;
};

struct VertexToPixelID {
	float4 position	:	SV_POSITION;
	float4 color	:	COLOR;
	float2 uv		:	UV;
};

struct PixelShaderOutputID
{
	int4 id : SV_Target0;
};
