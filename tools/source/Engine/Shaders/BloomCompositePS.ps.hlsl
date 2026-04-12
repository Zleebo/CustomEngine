cbuffer PostProcessBuffer : register(b0) {
	float myExposure;
	float myBloomThreshold;
	float myBloomStrength;
	int   myBloomEnabled;
};

Texture2D sceneTexture : register(t0);
Texture2D bloomTexture : register(t1);
SamplerState defaultSampler : register(s0);

struct PixelInput {
	float4 position : SV_POSITION;
	float2 uv       : UV;
};

float3 ACESFilm(float3 x) {
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return saturate((x*(a*x+b))/(x*(c*x+d)+e));
}

float4 main(PixelInput input) : SV_TARGET {
	float3 scene = sceneTexture.Sample(defaultSampler, input.uv).rgb;
	float3 bloom = bloomTexture.Sample(defaultSampler, input.uv).rgb;

	float3 color = scene + bloom * myBloomStrength;
	color *= myExposure;
	color = ACESFilm(color);
	color = pow(max(color, 0.0001f), 1.0f / 2.2f);
	return float4(color, 1.0f);
}
