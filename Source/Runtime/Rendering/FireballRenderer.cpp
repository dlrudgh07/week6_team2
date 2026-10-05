#include "EnginePCH.h"
#include "FireballRenderer.h"
#include "RenderResourceManager.h"
#include "RenderPacket.h"
#include "FireballSceneData.h"

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
    PerFrameCB = FRenderCommand::CreateConstantBuffer(sizeof(FFireballConstants));
    PerObjCB = FRenderCommand::CreateConstantBuffer(sizeof(FMatrix));
    FireBallCB = FRenderCommand::CreateConstantBuffer(sizeof(FFireballConstants));
    
    // Pipeline Set
    PipelineState.Shader = Shader;
    PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    PipelineState.RasterizerState = ERasterizerState::SolidNone;
    PipelineState.BlendState = EBlendState::Additive;
    PipelineState.DepthStencilState = EDepthStencilState::Disabled;

    return true;
}

void FFireballRenderer::OnRender(FRHITexture2D* SceneDepthTexture,
                                 const FMatrix& ViewProj,
                                 const TArray<FFireballSceneData>& Fireballs)
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
    Frame.ScreenSize[0] = ;
    Frame.ScreenSize[1] = ;

    FRenderCommand::UpdateBufferData(PerFrameCB.get(), &Frame);
    FRenderCommand::BindConstantBuffer(0, PerFrameCB.get(),
        EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);
    
    FRenderCommand::BindVertexBuffer(SphereVB.get());
    FRenderCommand::BindIndexBuffer(SphereIB.get());

    for (const FFireballSceneData& F : Fireballs)
    {
        // CB b1
        FMatrix World = /* Scale(Radius) * Translation(F.Position) */;
        FRenderCommand::UpdateBufferData(PerObjectCB.get(), &World);
        FRenderCommand::BindConstantBuffer(1, PerObjectCB.get(),
            EShaderBindFlagBits::Vertex | EShaderBindFlagBits::Pixel);

        // CB b2
        FFireballConstants C;
        C.FireballPosition = F.Position;
        C.Radius = F.Radius;
        C.Color = F.Color;
        C.Intensity = F.Intensity;
        C.RadiusFallOff = F.RadiusFallOff;
        FRenderCommand::UpdateBufferData(FireballCB.get(), &C);
        FRenderCommand::BindConstantBuffer(2, FireballCB.get(), EShaderBindFlagBits::Pixel);

        FRenderCommand::DrawIndexed(SphereIndexCount);
    }

    // SRV 해제, RTV/DSV 복원, 참조 해제
    FRenderCommand::BindShaderResource(0, nullptr, EShaderBindFlagBits::Pixel);
    Context->OMSetRenderTargets(1, &CurrentRTV, CurrentDSV);
    if (CurrentRTV) CurrentRTV->Release();
    if (CurrentDSV) CurrentDSV->Release();


}


