cbuffer FrameBuffer : register(b0) {
	float4x4 to_camera;
	float4x4 to_projection;
	float3 camRight;
	float _pad0;
	float3 camUp;
	float _pad1;
};

struct ParticleToGeo {
	float4 position : SV_POSITION;
	float4 color    : COLOR;
	float  size     : PSIZE;
};

struct GeoToPixel {
	float4 position : SV_POSITION;
	float4 color    : COLOR;
	float2 uv       : UV;
};

[maxvertexcount(4)]
void main(point ParticleToGeo input[1], inout TriangleStream<GeoToPixel> stream) {
	float3 center = input[0].position.xyz;
	float  s      = input[0].size * 0.5f;
	float4 color  = input[0].color;

	float3 corners[4] = {
		center + (-camRight - camUp) * s,
		center + ( camRight - camUp) * s,
		center + (-camRight + camUp) * s,
		center + ( camRight + camUp) * s,
	};

	float2 uvs[4] = { {0,1}, {1,1}, {0,0}, {1,0} };

	[unroll]
	for (int i = 0; i < 4; i++) {
		GeoToPixel o;
		float4 viewPos = mul(to_camera, float4(corners[i], 1.0f));
		o.position = mul(to_projection, viewPos);
		o.color    = color;
		o.uv       = uvs[i];
		stream.Append(o);
	}
}
