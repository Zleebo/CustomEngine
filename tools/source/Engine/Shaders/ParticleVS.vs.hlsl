cbuffer FrameBuffer : register(b0) {
	float4x4 to_camera;
	float4x4 to_projection;
	float3 camRight;
	float _pad0;
	float3 camUp;
	float _pad1;
};

struct ParticleInput {
	float3 position : POSITION;
	float4 color    : COLOR;
	float  size     : PSIZE;
};

struct ParticleToGeo {
	float4 position : SV_POSITION;
	float4 color    : COLOR;
	float  size     : PSIZE;
};

ParticleToGeo main(ParticleInput input) {
	ParticleToGeo output;
	output.position = float4(input.position, 1.0f);
	output.color    = input.color;
	output.size     = input.size;
	return output;
}
