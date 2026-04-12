struct VertexInput {
	float4 position : POSITION;
	float2 uv       : UV;
};

struct VertexToPixel {
	float4 position : SV_POSITION;
	float2 uv       : UV;
};

VertexToPixel main(VertexInput input) {
	VertexToPixel output;
	output.position = input.position;
	output.uv       = input.uv;
	return output;
}
