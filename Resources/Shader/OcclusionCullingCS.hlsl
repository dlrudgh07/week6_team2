struct FInstanceBound
{
    float3 Center;
    float Radius;
    float3 Extent;
    uint Pad;
};

StructuredBuffer<FInstanceBound> Instances : register(t0);
Texture2D<float> HZBTexture : register(t1);
SamplerState PointClampSampler : register(s0);

RWStructuredBuffer<uint> VisibilityBits : register(u0);

cbuffer CullConstants : register(b0)
{
    float4 FrustumPlanes[6];
    row_major float4x4 ViewProjection;
    float3 CameraPosition;
    float Pad0;
    float2 HZBSize;
    uint NumInstances;
    uint NumWords;
    uint bUseHZB;
    uint NumHZBMips;
    float DepthBias;
    float Pad;
};

groupshared uint s_ThreadBits[64];

[numthreads(64, 1, 1)]
void mainCS(uint3 GroupThreadID : SV_GroupThreadID, uint3 GroupID : SV_GroupID, uint3 DispatchThreadID : SV_DispatchThreadID)
{
    s_ThreadBits[GroupThreadID.x] = 0;
    GroupMemoryBarrierWithGroupSync();

    uint Index = DispatchThreadID.x;
    bool bVisible = false;
    uint LodCode = 0;

    if (Index < NumInstances)
    {
        FInstanceBound Bound = Instances[Index];
        float3 Center = Bound.Center;
        float3 Extent = Bound.Extent;

        bVisible = true;
        [unroll]
        for (int i = 0; i < 6; ++i)
        {
            float4 Plane = FrustumPlanes[i];
            float Dist = dot(Plane.xyz, Center) + Plane.w;
            float Radius = dot(abs(Plane.xyz), Extent);
            if (Dist < -Radius)
            {
                bVisible = false;
                break;
            }
        }

        // HZB 투영보다 싼 거리 기반 LOD를 먼저 판정해 아주 작은 물체는 조기에 제외한다.
        if (bVisible)
        {
            float Dist = length(Center - CameraPosition);
            float ScreenDiameter = (Bound.Radius * 2.0f) / max(Dist, 0.001f);

            if (ScreenDiameter < 0.01f)
            {
                LodCode = 0u;
            }
            else if (ScreenDiameter < 0.05f)
            {
                LodCode = 3u;
            }
            else if (ScreenDiameter < 0.15f)
            {
                LodCode = 2u;
            }
            else
            {
                LodCode = 1u;
            }
        }

        if (bVisible && LodCode > 0u && bUseHZB != 0)
        {
            // 가림 판정용 영역 투영
            float3 CullExtent = Extent * 0.85f;
            float4 CenterClip = mul(float4(Center, 1.0f), ViewProjection);
            float4 VX = ViewProjection[0] * CullExtent.x;
            float4 VY = ViewProjection[1] * CullExtent.y;
            float4 VZ = ViewProjection[2] * CullExtent.z;

            float4 CornersClip[8];
            CornersClip[0] = CenterClip - VX - VY - VZ;
            CornersClip[1] = CenterClip + VX - VY - VZ;
            CornersClip[2] = CenterClip - VX + VY - VZ;
            CornersClip[3] = CenterClip + VX + VY - VZ;
            CornersClip[4] = CenterClip - VX - VY + VZ;
            CornersClip[5] = CenterClip + VX - VY + VZ;
            CornersClip[6] = CenterClip - VX + VY + VZ;
            CornersClip[7] = CenterClip + VX + VY + VZ;

            float3 MinNDC = float3(1e9f, 1e9f, 1e9f);
            float3 MaxNDC = float3(-1e9f, -1e9f, -1e9f);
            bool bNearClipped = false;

            [unroll]
            for (int c = 0; c < 8; ++c)
            {
                float4 Clip = CornersClip[c];
                if (Clip.w <= 0.001f)
                {
                    bNearClipped = true;
                    break;
                }
                float3 NDC = Clip.xyz / Clip.w;
                MinNDC = min(MinNDC, NDC);
                MaxNDC = max(MaxNDC, NDC);
            }

            if (!bNearClipped)
            {
                float2 MinUV = float2(MinNDC.x * 0.5f + 0.5f, -MaxNDC.y * 0.5f + 0.5f);
                float2 MaxUV = float2(MaxNDC.x * 0.5f + 0.5f, -MinNDC.y * 0.5f + 0.5f);
                MinUV = clamp(MinUV, 0.0f, 1.0f);
                MaxUV = clamp(MaxUV, 0.0f, 1.0f);

                float2 PixelSize = (MaxUV - MinUV) * HZBSize;
                float MaxDim = max(PixelSize.x, PixelSize.y);
                float Mip = ceil(log2(max(MaxDim * 0.5f, 1.0f)));
                Mip = clamp(Mip, 0.0f, float(NumHZBMips - 1));

                // 텍셀 해상도 산출
                float2 MipSize = max(HZBSize * exp2(-Mip), 1.0f);
                int2 PixelMin = clamp(int2(MinUV * MipSize), int2(0, 0), int2(MipSize) - 1);
                int2 PixelMax = clamp(int2(MaxUV * MipSize), int2(0, 0), int2(MipSize) - 1);

                // 밉 텍셀 직접 조회
                float d0 = HZBTexture.Load(int3(PixelMin.x, PixelMin.y, int(Mip)));
                float d1 = HZBTexture.Load(int3(PixelMax.x, PixelMin.y, int(Mip)));
                float d2 = HZBTexture.Load(int3(PixelMin.x, PixelMax.y, int(Mip)));
                float d3 = HZBTexture.Load(int3(PixelMax.x, PixelMax.y, int(Mip)));

                // 가림 여부 판정
                float MaxHZBDepth = max(max(d0, d1), max(d2, d3));
                if (MinNDC.z > MaxHZBDepth + DepthBias)
                {
                    bVisible = false;
                }
            }
        }
    }

    // 비트 인코딩 및 공유 메모리 저장
    uint MyBits = 0;
    if (bVisible && LodCode > 0u)
    {
        uint LocalShift = (GroupThreadID.x % 16) * 2;
        MyBits = LodCode << LocalShift;
    }
    s_ThreadBits[GroupThreadID.x] = MyBits;

    GroupMemoryBarrierWithGroupSync();

    // 병렬 비트 병합 및 결과 기록
    if (GroupThreadID.x < 4)
    {
        uint Base = GroupThreadID.x * 16;
        uint FinalWord = s_ThreadBits[Base]      | s_ThreadBits[Base + 1]  |
                         s_ThreadBits[Base + 2]  | s_ThreadBits[Base + 3]  |
                         s_ThreadBits[Base + 4]  | s_ThreadBits[Base + 5]  |
                         s_ThreadBits[Base + 6]  | s_ThreadBits[Base + 7]  |
                         s_ThreadBits[Base + 8]  | s_ThreadBits[Base + 9]  |
                         s_ThreadBits[Base + 10] | s_ThreadBits[Base + 11] |
                         s_ThreadBits[Base + 12] | s_ThreadBits[Base + 13] |
                         s_ThreadBits[Base + 14] | s_ThreadBits[Base + 15];

        uint OutIndex = GroupID.x * 4 + GroupThreadID.x;
        if (OutIndex < NumWords)
        {
            VisibilityBits[OutIndex] = FinalWord;
        }
    }
}
