#include "ShaderStructs.hlsli"

PixelOutput main(VertexToPixel input) {
	PixelOutput result;
	float4 texture_color = _instance_texture.Sample(_default_sampler, input.uv.xy).rgba;
	
	result.color = texture_color;
	
	return result;
}