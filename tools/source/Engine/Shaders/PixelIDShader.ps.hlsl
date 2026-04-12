#include "ShaderIDStructs.hlsli"

PixelShaderOutputID main(VertexToPixelID input) {
	PixelShaderOutputID output;
	output.id = id;

	return output;
}