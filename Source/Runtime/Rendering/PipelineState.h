#pragma once

#include "Shader.h"
#include "RenderStates.h"


class FPipelineState
{
public:
	FPipelineState() = default;
	FPipelineState(FShaderProgram* InShader, D3D11_PRIMITIVE_TOPOLOGY InTopology,
		ERasterizerState InRS, EBlendState InBS, EDepthStencilState InDSS)
		: Shader(InShader), Topology(InTopology), RasterizerState(InRS), BlendState(InBS), DepthStencilState(InDSS) {}
	~FPipelineState() = default;

	FShaderProgram* Shader = nullptr;
	D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	ERasterizerState         RasterizerState = ERasterizerState::SolidBack;
	EBlendState              BlendState = EBlendState::Opaque;
	EDepthStencilState       DepthStencilState = EDepthStencilState::Default;
};