#include "ShaderIDStructs.hlsli"

VertexToPixelID main(VertexIDInput input) {
	VertexToPixelID result;

	float4 vtx_object_pos		= input.position;
	float4 vtx_world_pos		= mul(to_world, vtx_object_pos);
	float4 vtx_view_pos			= mul(to_camera, vtx_world_pos);
	float4 vtx_projection_pos	= mul(to_projection, vtx_view_pos);

	result.position = vtx_projection_pos;
	result.color = input.color;
	result.uv = input.uv;
	return result;
}