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
    //input.clip = 5;
    PixelOutput result;
    float4 rockColor = RockColor.Sample(aSampler, input.uv.xy).rgba;
    float4 rockNormal = RockNormal.Sample(aSampler, input.uv.xy).rgba;    
    float4 grassColor = GrassColor.Sample(aSampler, input.uv.xy).rgba;
    float4 grassNormal = GrassNormal.Sample(aSampler, input.uv.xy).rgba;
    float4 snowColor = SnowColor.Sample(aSampler, input.uv.xy).rgba;
    float4 snowNormal = SnowNormal.Sample(aSampler, input.uv.xy).rgba;
    float3 snowNormalE = expandNormal(snowNormal);
    float3 grassNormalE = expandNormal(grassNormal);
    float3 rockNormalE = expandNormal(rockNormal);
    float3 grassMaterial = GrassMaterial.Sample(aSampler, input.uv);
    float3 rockMaterial = RockMaterial.Sample(aSampler, input.uv);
    float3 snowMaterial = SnowMaterial.Sample(aSampler, input.uv);
    float slopeBlend = smoothstep(0.7f, 1.0f, input.normal.y);
    float heightBlend = smoothstep(-0.05f, 0.25f, input.worldPosition.y - clipDist.y);
 
    float3x3 TBN = float3x3(
normalize(input.tangent.xyz),
normalize(-input.binormal.xyz),
normalize(input.normal.xyz));
    TBN = transpose(TBN);
    float3 toEye = normalize(CameraPosition.xyz - input.worldPosition.xyz);
    float3 color = lerp(rockColor, lerp(grassColor, snowColor, heightBlend), slopeBlend).rgb;
    float4 albedo = lerp(rockColor, lerp(grassColor, snowColor, heightBlend), slopeBlend).rgba;
    if (albedo.a < 0.5)
    {
        discard;
        result.color = float4(0.f, 0.f, 0.f, 0.f);
        return result;
    }

    //if (input.clip < 0.0)
    //{
    //    discard;
    //    result.color = float4(0.f, 0.f, 0.f, 0.f);
    //    return result;
    //}
    float3 normal = lerp(rockNormalE, lerp(grassNormalE, snowNormalE, heightBlend), slopeBlend);
    float ambientOcclusion = lerp(rockNormalE.b, lerp(grassNormalE.b, snowNormalE.b, heightBlend), slopeBlend);
    float3 pixelNormal = normalize(mul(TBN, normal));

    

    float3 material = lerp(rockMaterial, lerp(grassMaterial, snowMaterial, heightBlend), slopeBlend);
    
    float metalness = material.r;
    float roughness = material.g;
    float emissive = material.b;
       
    float3 specularColor = lerp((float3) 0.04f, albedo.rgb, metalness);
    float3 diffuseColor = lerp((float3) 0.00f, albedo.rgb, 1 - metalness);


    float3 ambiance = EvaluateAmbiance(
		environmentTexture, pixelNormal, input.normal.xyz,
		toEye, roughness,
		ambientOcclusion, diffuseColor, specularColor
	);
    float3 directional = EvaluateDirectionalLight(
		diffuseColor, specularColor, pixelNormal, roughness,
		dirLightColour.xyz, dirLight.xyz, toEye.xyz
	);
    //float3 directional = (color * dirLightColour.rgb * max(0.f, dot(pixelNormal, dirLight.rgb)));




    
    
    
    float3 emissiveAlbedo = albedo.rgb * emissive;
    float3 radiance = ambiance + directional + emissiveAlbedo;
    result.color.rgb = tonemap_s_gamut3_cine(radiance);
     result.color.a = 1.0f;
   return result;
    

   // result.color = RockColor.Sample(aSampler, input.uv.xy).rgba;

}