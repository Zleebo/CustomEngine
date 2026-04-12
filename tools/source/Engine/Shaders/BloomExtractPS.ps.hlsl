cbuffer PostProcessBuffer : register(b0) {
	float myExposure;
	float myBloomThreshold;
	float myBloomStrength;
	int   myBloomEnabled;
};

Texture2D sceneTexture : register(t0);
SamplerState defaultSampler : register(s0);

struct PixelInput {
	float4 position : SV_POSITION;
	float2 uv       : UV;
};

float4 main(PixelInput input) : SV_TARGET {
	float3 color = sceneTexture.Sample(defaultSampler, input.uv).rgb;
	float brightness = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
	float3 bright = (brightness > myBloomThreshold) ? color : float3(0, 0, 0);
	return float4(bright, 1.0f);
}
