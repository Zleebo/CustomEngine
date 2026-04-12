#ifndef SHADOW_COMMON_HLSLI
#define SHADOW_COMMON_HLSLI

static const int kShadowCascadeCount = 4;

struct ShadowCascadeData
{
    float4x4 lightViewProjection[kShadowCascadeCount];
    float4 cascadeSplits;
};

#endif
