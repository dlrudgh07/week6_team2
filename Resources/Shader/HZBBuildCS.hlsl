Texture2D<float> SourceDepth : register(t0);
RWTexture2D<float> DestHZB : register(u0);
SamplerState PointClampSampler : register(s0);

[numthreads(16, 16, 1)]
void mainCS(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    uint2 DestSize;
    uint2 SourceSize;
    DestHZB.GetDimensions(DestSize.x, DestSize.y);
    SourceDepth.GetDimensions(SourceSize.x, SourceSize.y);

    if (DispatchThreadID.x >= DestSize.x || DispatchThreadID.y >= DestSize.y)
        return;

    float MaxDepth;

    // 첫 패스 여부 자동 판별
    if (SourceSize.x != DestSize.x * 2 || SourceSize.y != DestSize.y * 2)
    {
        float2 Scale = float2(SourceSize) / float2(DestSize);
        uint2 SrcCoord = uint2(float2(DispatchThreadID.xy) * Scale);
        uint2 Step = max(uint2(1, 1), uint2(Scale * 0.5f));
        float2 UV = (float2(SrcCoord) + float2(Step)) / float2(SourceSize);
        float4 Depths = SourceDepth.GatherRed(PointClampSampler, UV);
        MaxDepth = max(max(Depths.x, Depths.y), max(Depths.z, Depths.w));
    }
    else
    {
        // 텍셀 수집 명령어로 깊이 일괄 조회
        float2 UV = (float2(DispatchThreadID.xy * 2) + 1.0f) / float2(SourceSize);
        float4 Depths = SourceDepth.GatherRed(PointClampSampler, UV);
        MaxDepth = max(max(Depths.x, Depths.y), max(Depths.z, Depths.w));
    }

    DestHZB[DispatchThreadID.xy] = MaxDepth;
}
