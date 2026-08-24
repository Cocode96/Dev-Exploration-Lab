struct EFFECT_UPDATE_DATA
{
    float4 vPosition;
    float4 vVelocity;
    float4 vRotation;
    float4 vAngularVelocity;
    float4 vScaleLife;
    row_major float4x4 matWorld;
};

cbuffer FRAME_DESC : register(b0)
{
    float g_fTimeDelta;
    uint g_iUpdateCount;
    float2 g_vPadding;
};

RWStructuredBuffer<EFFECT_UPDATE_DATA> g_UpdateDatas : register(u0);

[numthreads(256, 1, 1)]
void CS_MAIN(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    uint iIndex = DispatchThreadID.x;

    // Dispatch 그룹을 256개 단위로 올림하기 때문에 남는 스레드는 제외한다.
    if (iIndex >= g_iUpdateCount)
        return;

    EFFECT_UPDATE_DATA Data = g_UpdateDatas[iIndex];

    Data.vVelocity.xyz += float3(0.f, -9.8f, 0.f) * g_fTimeDelta;
    Data.vPosition.xyz += Data.vVelocity.xyz * g_fTimeDelta;
    Data.vRotation.xyz += Data.vAngularVelocity.xyz * g_fTimeDelta;
    Data.vScaleLife.w += g_fTimeDelta;

    float fCos = cos(Data.vRotation.z);
    float fSin = sin(Data.vRotation.z);
    float4x4 matScale = float4x4(
        Data.vScaleLife.x, 0.f, 0.f, 0.f,
        0.f, Data.vScaleLife.y, 0.f, 0.f,
        0.f, 0.f, Data.vScaleLife.z, 0.f,
        0.f, 0.f, 0.f, 1.f);
    float4x4 matRotation = float4x4(
        fCos, fSin, 0.f, 0.f,
        -fSin, fCos, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f);
    float4x4 matTranslation = float4x4(
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        Data.vPosition.x, Data.vPosition.y, Data.vPosition.z, 1.f);

    Data.matWorld = mul(mul(matScale, matRotation), matTranslation);
    g_UpdateDatas[iIndex] = Data;
}
