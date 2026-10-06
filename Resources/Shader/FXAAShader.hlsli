static const float EDGE_THRESHOLD = 0.166;
static const float EDGE_THRESHOLD_MIN = 0.0833;
static const float SUBPIXEL_QUALITY = 0.75;
static const float STEPS[] = { 1, 1, 1, 1, 1, 1.5, 2, 2, 2, 2, 4, 8 };
static const int STEPSLENGTH = 12;
/*
NW N NE
W  M  E
SW S SE
*/


struct Position
{
    float maxSM;
    float minSM;
     float maxE;
};
    
float Luma(float4 rgba)
{
    return dot(rgba.rgb, float3(0.299, 0.587, 0.114)); 
}
float LumaMax(float M, float N, float S, float W, float E)
{
    return max(M, max(max(max(N, S), W), E));
}
float LumaMin(float M, float N, float S, float W, float E)
{
    return min(M, min(min(min(N, S), W), E));
}

