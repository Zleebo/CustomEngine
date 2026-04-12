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
float3 s_curve(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
float3 tonemap_s_gamut3_cine(float3 c)
{
    // based on Sony's s gamut3 cine
    float3x3 fromSrgb = float3x3(
        +0.6456794776, +0.2591145470, +0.0952059754,
        +0.0875299915, +0.7596995626, +0.1527704459,
        +0.0369574199, +0.1292809048, +0.8337616753);

    float3x3 toSrgb = float3x3(
        +1.6269474099, -0.5401385388, -0.0868088707,
        -0.1785155272, +1.4179409274, -0.2394254004,
        +0.0444361150, -0.1959199662, +1.2403560812);

    return mul(toSrgb, s_curve(mul(fromSrgb, c)));
}

float3 expandNormal(float4 normalTexture)
{
    float3 normal = normalTexture.agg;
    normal = 2.0f * normal - 1.0f;
    normal.z = sqrt(1 - saturate(normal.x * normal.x + normal.y * normal.y));
    return normalize(normal);
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
   
    float clip : SV_ClipDistance0;
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
struct PixelOutput
{
    float4 color : SV_TARGET;
};
PixelOutput main(PixelInputType input)
{
  //  input.clip = 5;
    PixelOutput result;
 
    float4 color;
    float3x3 TBN = float3x3(
normalize(input.tangent.xyz),
normalize(-input.binormal.xyz),
normalize(input.normal.xyz));
    TBN = transpose(TBN);
    
    float3 toEye = normalize(CameraPosition.xyz - input.worldPosition.xyz);
  
    
    
    float fresnel = Fresnel_Schlick(
    float3(0.25f, 0.25f, 0.25f),
    float3(0.0f, 1.0f, 0.0f),
    toEye).r;
    
    float width, height;
    WaterReflection.GetDimensions(width, height);
    float2 resolution = float2(width, height);

    float3 reflection = WaterReflection.Sample(aSampler, input.position.xy / resolution).rgb;
    result.color.rgb = fresnel * reflection;
    result.color.a = 1.f;
    return result;

}