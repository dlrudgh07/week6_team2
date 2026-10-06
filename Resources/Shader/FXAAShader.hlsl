#include "FXAAShader.hlsli"

cbuffer FXAAConstants : register(b0)
{
    float2 texel;
//    float EDGE_THRESHOLD;
//    float EDGE_THRESHOLD_MIN;
//    float SUBPIXEL_QUALITY;
//    int STEPS[];
//    int STEPSLENGTH;
};
struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

Texture2D Texture : register(t0);
// Texture2D Texture : register(t1);
SamplerState LinearSampler : register(s0);
// SamplerState LinearSampler : register(s1);

// Vertex Shader
// Vertex Buffer, Index Buffer 불필요 (Ex. Draw(3, 0))
PS_INPUT mainVS(uint ID : SV_VertexID)
{
    PS_INPUT Output;
    // (0, 0), (2, 0), (0, 2)
    Output.UV = float2((ID << 1) & 2, (ID & 2));

    // (-1, 1), (3, 1), (-1, -3) -> 화면 전체를 가리는 큰 삼각형 하나
    Output.Position = float4(Output.UV * float2(2, -2) + float2(-1, 1), 0, 1);

    return Output;
}
/*
NW N NE
W  M  E
SW S SE
*/

// Pixel Shader
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    
    float4 Color = Texture.Sample(LinearSampler, input.UV);
    
    //STEP 1) 주변밝기측정
    float M = Luma(Color);
    float N = Luma(Texture.Sample(LinearSampler, input.UV + float2(0, -1) * texel));
    float S = Luma(Texture.Sample(LinearSampler, input.UV + float2(0, 1) * texel));
    float W = Luma(Texture.Sample(LinearSampler, input.UV + float2(-1, 0) * texel));
    float E = Luma(Texture.Sample(LinearSampler, input.UV + float2(1, 0) * texel));

    float lumaMax = LumaMax(M, N, S, W, E);
    float lumaMin = LumaMin(M, N, S, W, E);
    float diffLuma = lumaMax - lumaMin;
    
    //STEP 2) 경계인지 확인
    
    //주변 밝기차가 작을때
    //EDGE_THRESHOLD_MIN = 너무 어두울 때 방지
    //lumaMax * EDGE_THRESHOLD = 밝기차가 기대에 못미칠때
    if (diffLuma < max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD))
    {
        return Color;
    }
    
    //대각선방향 밝기측정
    float NW = Luma(Texture.Sample(LinearSampler, input.UV + float2(-1, -1) * texel));
    float SW = Luma(Texture.Sample(LinearSampler, input.UV + float2(-1, 1) * texel));
    float NE = Luma(Texture.Sample(LinearSampler, input.UV + float2(1, -1) * texel));
    float SE = Luma(Texture.Sample(LinearSampler, input.UV + float2(1, 1) * texel));

    //세로,가로방향 밝기차 측정
    float edgeHorz = abs(NW + SW - W * 2) + abs(N + S - M * 2) * 2 + abs(NE + SE - E * 2);
    float edgeVert = abs(NW + NE - N * 2) + abs(W + E - M * 2) * 2 + abs(SW + SE - S * 2);
    bool isHorizontal = edgeHorz >= edgeVert; // 수평방향으로 밝기가 나뉘는지
    
    
    /*
    000 Horizontal=true
    111
    000
    */
    
    float lumaNW = isHorizontal ? N : W;
    float lumaSE = isHorizontal ? S : E;
    
    //steep = 가파른
    //gradient = 기울기
    bool NWisSteeper = abs(lumaNW - M) >= abs(lumaSE - M); // true라면 왼쪽윗방향 밝기차 높음
    float gradient = max(abs(lumaNW - M), abs(lumaSE - M)); 
    float threshold = gradient * 0.25; 
    
    float lumaLocalAvg;
    
    //왼쪽위 방향이 가파를때
    if (NWisSteeper)
    {
        lumaLocalAvg = (M + lumaNW) * 0.5;
    }
    else
    {
        lumaLocalAvg = (M + lumaSE) * 0.5;
    }
    
    float2 offsetDirect = isHorizontal ? float2(0, texel.y) : float2(texel.x, 0);

    if (NWisSteeper)
        offsetDirect = -offsetDirect;
    float2 boundaryDirect = isHorizontal ? float2(texel.x, 0) : float2(0, texel.y);

    float2 uvNeg = input.UV + offsetDirect * 0.5 - boundaryDirect * STEPS[0];
    float2 uvPos = input.UV + offsetDirect * 0.5 + boundaryDirect * STEPS[0];
    bool doneNeg = false; // Negative 방향의 끝이 밝기변화가 심한가?
    bool donePos = false; // Positive 방향의 끝이 밝기변화가 심한가?
    float endNeg; // Negative 방향의 밝기
    float endPos; // Positive 방향의 밝기
    
    for (int i = 1; i < STEPSLENGTH; ++i)
    {
        if (!doneNeg)
            endNeg = Luma(Texture.Sample(LinearSampler, uvNeg)) - lumaLocalAvg;
        if (!donePos)
            endPos = Luma(Texture.Sample(LinearSampler, uvPos)) - lumaLocalAvg;
        
        doneNeg = abs(endNeg) >= threshold;
        donePos = abs(endPos) >= threshold;
        
        if (!doneNeg)
            uvNeg -= boundaryDirect * STEPS[i];
        if (!donePos)
            uvPos += boundaryDirect * STEPS[i];
        
        if (doneNeg && donePos)
            break;
    }
    float distNeg = isHorizontal ? abs(input.UV.x - uvNeg.x) : abs(input.UV.y - uvNeg.y);
    float distPos = isHorizontal ? abs(uvPos.x - input.UV.x) : abs(uvPos.y - input.UV.y);
    
    float pixeloffset = 0.5 - min(distNeg, distPos) / (distNeg + distPos);
    bool isMDarker = M < lumaLocalAvg; // 내가 주변보다 어두운지 밝은지

    float endNearer;
    if (distNeg < distPos)
    {
        endNearer = endNeg;
    }
    else
    {
        endNearer = endPos;
    }

    
    float edgeOffset;
    if (!isMDarker && endNearer < 0) // 내가 밝은편인데 끝에서 어두워짐 -> 내줄이 어두워짐
    {
        edgeOffset = pixeloffset;
    }
    else if (isMDarker && endNearer >= 0) // 나는 어두운데 끝에서 밝아짐 -> 내줄이 밝아짐
    {
        edgeOffset = pixeloffset;
    }
    else //내가 아니고 반대편에서 처리해야됨
    {
        edgeOffset = 0;
    }
    
    float avg = ((N + S + W + E) * 2 + (NW + NE + SW + SE)) / 12;
    float sub = saturate(abs(avg - M) / diffLuma);
    sub = sub * sub * (3 - 2 * sub);
    float subOffset = sub * sub * SUBPIXEL_QUALITY;

    float finalOffset = max(edgeOffset, subOffset);
    float2 finalUV = input.UV + offsetDirect * finalOffset;

    return Texture.Sample(LinearSampler, finalUV);

}
