cbuffer TerrainFrameBuffer : register(b0)
{
    float4x4 worldToClipMatrix;
    float4 deltaTime;
    float4 CameraPosition;
}

struct VSInput
{
    float4 position : POSITION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(worldToClipMatrix, input.position);
    return output;
}
