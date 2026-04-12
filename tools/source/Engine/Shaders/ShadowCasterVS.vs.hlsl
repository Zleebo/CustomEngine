#include "ShaderStructs.hlsli"

struct ShadowVertexToPixel
{
    float4 position : SV_POSITION;
};

ShadowVertexToPixel main(VertexInput input)
{
    ShadowVertexToPixel result;
    float4 worldPos = mul(to_world, input.position);
    float4 viewPos = mul(to_camera, worldPos);
    result.position = mul(to_projection, viewPos);
    return result;
}
