#include "EnginePCH.h"
#include "AntiAliasingRenderer.h"
#include "RenderResourceManager.h"
#include "RenderCommand.h"

bool FAntiAliasingRenderer::Init()
{
	// 셰이더 가져오기
	Shader = FRenderResourceManager::GetShaderProgram("Resources/Shader/FXAAShader.hlsl");
	if (!Shader)
	{
		LOG(Error, "[FXAA] shader not found");
		return false;
	}

	//Desc 필요없음

	//상수버퍼 만들기
	ConstantBuffer = FRenderCommand::CreateConstantBuffer(sizeof(FAntiAliasingConstants));
	
	//Pipeline Set
	PipelineState.Shader = Shader;
	PipelineState.Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	PipelineState.RasterizerState = ERasterizerState::SolidNone;
	PipelineState.BlendState = EBlendState::Opaque;
	PipelineState.DepthStencilState = EDepthStencilState::Disabled;
	
	return true;
}

void FAntiAliasingRenderer::OnRender(const FRenderingInfo& ViewRenderingInfo)
{
	if (!IsValid())
		return;
	FRHITexture2D* SceneColor = ViewRenderingInfo.SceneColorTarget.Texture; // 입력: 장면이 그려진 중간 텍스처
	FRHITexture2D* DepthStencil = ViewRenderingInfo.DepthSteincil.Texture;  // ⑥에서 다시 붙일 깊이
	if (!SceneColor || ViewRenderingInfo.ColorRenderTargets.Num() == 0)
		return;
	FRHITexture2D* ColorTarget = ViewRenderingInfo.ColorRenderTargets[0].Texture; // 출력: ImGui가 표시하는 텍스처

	const uint32 Width = ViewRenderingInfo.ViewportSetting.Width;
	const uint32 Height = ViewRenderingInfo.ViewportSetting.Height;
	if (!ColorTarget || Width == 0 || Height == 0)
		return;
	FRenderCommand::SetRenderTarget(ColorTarget,nullptr);             // ① 출력을 ColorTarget으로 (SceneColor 떼기)
	FRenderCommand::BindShaderResource(0, SceneColor, EShaderBindFlagBits::Pixel); // ② SceneColor를 입력으로

	//SRV 바인딩

	FAntiAliasingConstants Constants;
	Constants.Texel[0] = 1 / static_cast<float>(ViewRenderingInfo.ViewportSetting.Width); //Texel X
	Constants.Texel[1] = 1 / static_cast<float>(ViewRenderingInfo.ViewportSetting.Height); // Texel Y

	FRenderCommand::BindPipelineState(&PipelineState);
	FRenderCommand::UpdateBufferData(ConstantBuffer.get(), &Constants);
	FRenderCommand::BindConstantBuffer(0, ConstantBuffer.get(), EShaderBindFlagBits::Pixel);
	FRenderCommand::BindSamplerState(0, ESamplerState::LinearClamp, EShaderBindFlagBits::Pixel);
	// 정점 버퍼 없이 3개. VS가 SV_VertexID로 삼각형을 만든다.
	FRenderCommand::Draw(3);

	//원상복구단계


	//RTV원상복구
	
	// 
	//SRV 바인드 해제
	FRenderCommand::PSSetShaderResource(0, nullptr);
	FRenderCommand::SetRenderTarget(ColorTarget, DepthStencil);
	return;
}
