struct VertexInput {
	float2 position : POSITION;
	float2 uv       : TEXCOORD;
	float4 color    : COLOR;
};

struct VertexToPixel {
	float4 position : SV_POSITION;
	float2 uv       : TEXCOORD;
	float4 color    : COLOR;
};

VertexToPixel main(VertexInput input) {
	VertexToPixel output;
	output.position = float4(input.position, 0.0f, 1.0f);
	output.uv       = input.uv;
	output.color    = input.color;
	return output;
}
