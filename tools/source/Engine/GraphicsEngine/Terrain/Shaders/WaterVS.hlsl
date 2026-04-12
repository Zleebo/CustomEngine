#include "PBRFunctions.hlsli"
Texture2D aTexture : register(t0);
Texture2D GrassColor : register(t2);
Texture2D GrassNormal : register(t3);
Texture2D RockColor : register(t4);
Texture2D RockNormal : register(t5);
Texture2D SnowColor : register(t6);
Texture2D SnowNormal : register(t7);
TextureCube environmentTexture : register(t8);
Texture2D GrassMaterial : register(t9);
Texture2D SnowMaterial : register(t10);
Texture2D RockMaterial : register(t11);
Texture2D WaterReflection : register(t12);
cbuffer ObjectBuffer : register(b1)
{
    float4x4 modelToWorld;
    float4 clipDist;
}
cbuffer FrameBuffer : register(b0)
{
    float4x4 worldToClipMatrix;
    float4 deltaTime;
    float4 CameraPosition;
}
cbuffer skyBuffer : register(b2)
{
    float4 dirLight;
    float4 dirLightColour;
    float4 skyColour;
    float4 groundColour;
}
struct VertexInputType
{
    float3 position : POSITION;
    
    float3 normal : NORMAL;
   
    float2 uv : TEXCOORD;
    
    float3 tangent : TANGENT;
    
    float3 binormal : BINORMAL;

};

struct PixelInputType
{
    float3 worldPosition : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float clip : SV_ClipDistance0;
};
PixelInputType main(VertexInputType input)
{
    float3x3 rotationTransform = modelToWorld;
    PixelInputType output;
    
   //output.clip = 5500;
    output.tangent = mul(rotationTransform, input.tangent);
    output.normal = mul(rotationTransform, input.normal);
    output.binormal = mul(rotationTransform, input.binormal);
   
  //  float4 vertexObjectNormal = float4(input.normal, 0.0);
   // float4 vertexWorldNormal = mul(modelToWorld, vertexObjectNormal);
    float4 vertexObjectPos = float4(input.position, 1);
    float4 vertexWorldPos = mul(modelToWorld, vertexObjectPos);
    float4 vertexClipPos = mul(worldToClipMatrix, vertexWorldPos);
    output.position = vertexClipPos;
    output.worldPosition = vertexWorldPos;
    //output.normal = input.normal;
    output.clip = vertexObjectPos.y - clipDist.x;
    output.uv = input.uv;
    return output;
}