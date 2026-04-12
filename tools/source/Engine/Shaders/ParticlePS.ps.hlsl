struct GeoToPixel {
	float4 position : SV_POSITION;
	float4 color    : COLOR;
	float2 uv       : UV;
};

struct PixelOutput {
	float4 color : SV_TARGET;
};

PixelOutput main(GeoToPixel input) {
	PixelOutput output;

	float2 uv = input.uv * 2.0f - 1.0f;
	float dist = dot(uv, uv);
	if (dist > 1.0f) discard;

	float alpha = (1.0f - dist) * input.color.a;
	output.color = float4(input.color.rgb, alpha);
	return output;
}
