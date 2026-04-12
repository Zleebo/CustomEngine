Texture2D uiTexture : register(t0);
SamplerState defaultSampler : register(s0);

struct PixelInput {
	float4 position : SV_POSITION;
	float2 uv       : TEXCOORD;
	float4 color    : COLOR;
};

float4 main(PixelInput input) : SV_TARGET {
	float4 texColor = uiTexture.Sample(defaultSampler, input.uv);
	return texColor * input.color;
}
