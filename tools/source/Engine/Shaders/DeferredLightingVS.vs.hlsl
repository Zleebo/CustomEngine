struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

struct VSInput
{
    uint vertexID : SV_VertexID;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    float2 uv = float2((input.vertexID << 1) & 2, input.vertexID & 2);
    output.uv = uv;
    output.position = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}
