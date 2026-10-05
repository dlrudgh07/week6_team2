#pragma once

#include "RHI/RHIBuffer.h"
#include "Rendering/RenderCommand.h"
#include "RHI/PipelineState.h"
#include "Engine/Texture2D.h"

#include "Math/Color.h"
#include "FireballSceneData.h"
#include "Rendering/RenderingInfo.h"

struct FFireballFrameConstants
{
    FMatrix ViewProj;           // 64 bytes
    FMatrix InvViewProj;        // 64 bytes
    FVector2D ScreenSize;       // 8  bytes
    FVector2D Padding;          // 8  bytes
};

struct FFireballConstants
{
    FVector FireballPosition;     // 12
    float Radius;                 // 4

    FLinearColor Color;           // 16
    
    float Intensity;              // 4
    float RadiusFallOff;          // 4
    FVector2D Padding;            // 8
};

class FFireballRenderer
{
public:
    FFireballRenderer() = default;
    ~FFireballRenderer() = default;

    bool Init();
    bool IsValid();

    void OnRender(FRHITexture2D* SceneDepthTexture, const FMatrix& ViewProj, const TArray<FFireballSceneData>& Fireballs, const FViewportSettings& Viewport);

private:
    FShaderProgram* Shader = nullptr;
    
    TUniquePtr<FRHIUniformBuffer> PerFrameCB;
    TUniquePtr<FRHIUniformBuffer> PerObjCB;
    TUniquePtr<FRHIUniformBuffer> FireBallCB;

    FPipelineState PipelineState;
    UStaticMesh* SphereMesh = nullptr;
};

