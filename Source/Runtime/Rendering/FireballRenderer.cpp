#include "EnginePCH.h"
#include "FireballRenderer.h"
#include "RenderResourceManager.h"
#include "RenderPacket.h"
#include "FireballSceneData.h"
#include "Engine/AssetManager.h"

bool FFireballRenderer::IsValid()
{
	return Shader != nullptr && PerFrameCB && PerObjCB && FireBallCB && SphereMesh;
}

bool FFireballRenderer::Init()
{
    // Firball Shader Load
    Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/FireballShader.hlsl");
    if (!Shader)
    {
        LOG(Error, "[Fireball] Shader not found");
        return false;
    }

    // Constants Buffer
    PerFrameCB = FRenderCommand::CreateConstantBuffer(sizeof(FFireballFrameConstants));
    PerObjCB = FRenderCommand::CreateConstantBuffer(sizeof(FMatrix));
    FireBallCB = FRenderCommand::CreateConstantBuffer(sizeof(FFireballConstants));
        
    // Pipeline Set
    PipelineState.Shader = Shader;
    PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    PipelineState.RasterizerState = ERasterizerState::SolidFront;
    PipelineState.BlendState = EBlendState::Additive;
    PipelineState.DepthStencilState = EDepthStencilState::Disabled;
    
    SphereMesh = UAssetManager::GetAssetByKey<UStaticMesh>("Sphere");
   
    return true;
}

void FFireballRenderer::OnRender(FRHITexture2D* SceneDepthTexture,
                                const FMatrix& ViewProj,
                                const TArray<FFireballSceneData>& Fireballs,
                                const FViewportSettings& Viewport)
{
    if (!IsValid() || Fireballs.Num() == 0) return;

    ID3D11DeviceContext* Context = FRenderCommand::GetContext();
    if (!Context) return;

    // RTV
    ID3D11RenderTargetView* CurrentRTV[1] = { nullptr };
    
    // DSV
    ID3D11DepthStencilView* CurrentDSV = nullptr;

    // OM 에서 RSV, DSV 가져오기
    Context->OMGetRenderTargets(1, CurrentRTV, &CurrentDSV);

    // DSV 해제
    if (CurrentDSV) Context->OMSetRenderTargets(1, CurrentRTV, nullptr);

    FRenderCommand::BindPipelineState(&PipelineState);
    FRenderCommand::BindShaderResource(0, SceneDepthTexture, EShaderBindFlagBits::Pixel);
    FRenderCommand::BindSamplerState(0, ESamplerState::PointClamp, EShaderBindFlagBits::Pixel);

    // CB b0
    FFireballFrameConstants Frame;
    Frame.ViewProj = ViewProj;
    Frame.InvViewProj = ViewProj.Inverse();      
    Frame.ScreenSize[0] = Viewport.Width;
    Frame.ScreenSize[1] = Viewport.Height;

    FRenderCommand::UpdateBufferData(PerFrameCB.get(), &Frame, nullptr);
    FRenderCommand::BindConstantBuffer(0, PerFrameCB.get(), EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
        
    FRenderCommand::BindMesh(SphereMesh);

    for (const FFireballSceneData& F : Fireballs)
    {
        // CB b1
        FMatrix World = FMatrix::Identity;
        World.M[0][0] = F.Radius;
        World.M[1][1] = F.Radius;
        World.M[2][2] = F.Radius;
        World.SetOrigin(F.Position);    // [3][0], [3][1], [3][2]

        FRenderCommand::UpdateBufferData(PerObjCB.get(), &World);
        FRenderCommand::BindConstantBuffer(1, PerObjCB.get(),
                                        EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);

        // CB b2
        FFireballConstants C;
        C.FireballPosition = F.Position;
        C.Radius = F.Radius;
        C.Color = F.Color;
        C.Intensity = F.Intensity;
        C.RadiusFallOff = F.RadiusFallOff;
        FRenderCommand::UpdateBufferData(FireBallCB.get(), &C);
        FRenderCommand::BindConstantBuffer(2, FireBallCB.get(), EShaderBindFlagBits::Pixel);

        FRenderCommand::DrawIndexed(SphereMesh->GetIndexCount());
    }

    // SRV 해제, RTV/DSV 복원, 참조 해제
    FRenderCommand::PSSetShaderResource(0, nullptr);
    Context->OMSetRenderTargets(1, CurrentRTV, CurrentDSV);
    if (CurrentRTV[0]) CurrentRTV[0]->Release();
    if (CurrentDSV) CurrentDSV->Release();


}


