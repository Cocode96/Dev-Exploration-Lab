cbuffer FrameConstants : register(b0)
{
    float4x4 g_World;
    float4x4 g_ViewProjection;
    float4 g_CameraPosition;
    float4 g_CameraRight;
    float4 g_CameraUp;
    float4 g_CameraForward;
    float4 g_SkyZenith;
    float4 g_SkyHorizon;
    float2 g_Resolution;
    int g_AlphaMode;
    int g_DepthMode;
    float g_PAlpha;
    float g_KAlpha;
    float g_KDepth;
    float g_Padding;
};

struct EffectInput
{
    float2 localPosition : POSITION;
    float2 uv : TEXCOORD0;
    float4 positionAndBillboard : INSTANCE_POSITION;
    float2 size : INSTANCE_SIZE;
    float4 color : INSTANCE_COLOR;
    float2 rotation : INSTANCE_ROTATION;
};

struct EffectOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float depth : TEXCOORD1;
};

EffectOutput VSEffect(EffectInput input)
{
    EffectOutput output;
    const float sine = sin(input.rotation.y);
    const float cosine = cos(input.rotation.y);
    const float2 scaled = input.localPosition * input.size;
    const float2 rolled = float2(
        scaled.x * cosine - scaled.y * sine,
        scaled.x * sine + scaled.y * cosine);

    float3 offset;
    if (input.positionAndBillboard.w > 0.5f)
    {
        offset = g_CameraRight.xyz * rolled.x + g_CameraUp.xyz * rolled.y;
    }
    else
    {
        const float yawSine = sin(input.rotation.x);
        const float yawCosine = cos(input.rotation.x);
        offset = float3(rolled.x * yawCosine, rolled.y, -rolled.x * yawSine);
    }

    const float3 worldPosition = input.positionAndBillboard.xyz + offset;
    output.position = mul(float4(worldPosition, 1.0f), g_ViewProjection);
    output.uv = input.uv;
    output.color = input.color;
    output.depth = saturate(output.position.z / max(output.position.w, 1e-4f));
    return output;
}

float ParticleAlpha(float2 uv, float baseAlpha)
{
    const float radial = length(uv * 2.0f - 1.0f);
    const float softEdge = 1.0f - smoothstep(0.55f, 1.0f, radial);
    return saturate(baseAlpha * softEdge);
}

float CalcWeight(float alpha, float zNdc)
{
    const float clippedAlpha = max(1e-3f, saturate(alpha));
    const float alphaWeight = (g_AlphaMode == 0)
        ? pow(clippedAlpha, g_PAlpha) * g_KAlpha
        : 1.0f - exp(-clippedAlpha * g_KAlpha);

    const float depthInput = 1.0f - saturate(zNdc);
    const float depthWeight = (g_DepthMode == 0)
        ? 1.0f
        : exp(-depthInput * g_KDepth);
    return alphaWeight * depthWeight;
}

float4 PSAlpha(EffectOutput input) : SV_Target0
{
    const float alpha = ParticleAlpha(input.uv, input.color.a);
    clip(alpha - 0.002f);
    return float4(input.color.rgb, alpha);
}

struct WboitOutput
{
    float4 accumColor : SV_Target0;
    float accumWeight : SV_Target1;
};

WboitOutput PSWboit(EffectOutput input)
{
    WboitOutput output;
    const float alpha = ParticleAlpha(input.uv, input.color.a);
    clip(alpha - 0.002f);
    const float weight = CalcWeight(alpha, input.depth);
    output.accumColor = float4(input.color.rgb * alpha * weight, alpha);
    output.accumWeight = alpha * weight;
    return output;
}

struct WorldInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
};

struct WorldOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float3 worldPosition : TEXCOORD0;
};

WorldOutput VSWorld(WorldInput input)
{
    WorldOutput output;
    const float4 worldPosition = mul(float4(input.position, 1.0f), g_World);
    output.position = mul(worldPosition, g_ViewProjection);
    output.normal = normalize(mul(float4(input.normal, 0.0f), g_World).xyz);
    output.color = input.color;
    output.worldPosition = worldPosition.xyz;
    return output;
}

float4 PSTerrain(WorldOutput input) : SV_Target0
{
    const float3 lightDirection = normalize(float3(-0.35f, 0.85f, -0.25f));
    const float diffuse = saturate(dot(normalize(input.normal), lightDirection));
    const float gridX = 1.0f - smoothstep(0.92f, 1.0f, abs(frac(input.worldPosition.x) * 2.0f - 1.0f));
    const float gridZ = 1.0f - smoothstep(0.92f, 1.0f, abs(frac(input.worldPosition.z) * 2.0f - 1.0f));
    const float grid = saturate(gridX + gridZ) * 0.20f;
    const float3 litColor = input.color.rgb * (0.38f + 0.62f * diffuse) + grid;
    return float4(litColor, 1.0f);
}

struct FullscreenOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

FullscreenOutput VSFullscreen(uint vertexId : SV_VertexID)
{
    FullscreenOutput output;
    const float2 position = float2((vertexId << 1) & 2, vertexId & 2);
    output.uv = position;
    output.position = float4(position * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

float4 PSSky(FullscreenOutput input) : SV_Target0
{
    const float2 ndc = input.uv * 2.0f - 1.0f;
    const float aspect = g_Resolution.x / g_Resolution.y;
    const float3 ray = normalize(g_CameraForward.xyz
        + g_CameraRight.xyz * ndc.x * aspect * 0.72f
        - g_CameraUp.xyz * ndc.y * 0.72f);
    const float heightBlend = saturate(ray.y * 0.82f + 0.35f);
    float3 color = lerp(g_SkyHorizon.rgb, g_SkyZenith.rgb, heightBlend);
    const float sun = pow(saturate(dot(ray, normalize(float3(0.25f, 0.58f, 0.78f)))), 420.0f);
    color += float3(1.0f, 0.82f, 0.52f) * sun * 1.6f;
    return float4(color, 1.0f);
}

Texture2D<float4> g_AccumColor : register(t0);
Texture2D<float> g_AccumWeight : register(t1);

float4 PSResolve(FullscreenOutput input) : SV_Target0
{
    const int2 pixel = int2(input.position.xy);
    const float4 accumColor = g_AccumColor.Load(int3(pixel, 0));
    const float weightSum = g_AccumWeight.Load(int3(pixel, 0));
    if (accumColor.a < 0.001f || weightSum < 1e-4f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    const float3 weightedColor = accumColor.rgb / clamp(weightSum, 1e-4f, 5e4f);
    const float compositeAlpha = 1.0f - exp(-accumColor.a);
    return float4(weightedColor, saturate(compositeAlpha));
}
