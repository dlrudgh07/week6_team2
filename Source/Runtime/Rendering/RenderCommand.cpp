#include "EnginePCH.h"
#include "RenderCommand.h"

#include "RHI/PipelineState.h"
#include "RHI/RHIBuffer.h"
#include "Engine/StaticMesh.h"
#include "RHI/RHIShader.h"
#include "Engine/Texture2D.h"
#include "RHI/RHITextureCube.h"
#include "RenderingInfo.h"

void FRenderCommand::Init(FDynamicRHI* InRenderDevice)
{
	RenderDevice = InRenderDevice;
}

TUniquePtr<FRHIVertexBuffer> FRenderCommand::CreateStaticVertexBuffer(const void* InVertices, uint32 InSize, uint32 Stride)
{
	assert(RenderDevice);
	return RenderDevice->CreateStaticVertexBuffer(InVertices, InSize, Stride);
}

TUniquePtr<FRHIVertexBuffer> FRenderCommand::CreateDynamicVertexBuffer(uint32 MaxSize, uint32 Stride)
{
	assert(RenderDevice);
	return RenderDevice->CreateDynamicVertexBuffer(MaxSize, Stride);
}

TUniquePtr<FRHIIndexBuffer> FRenderCommand::CreateStaticIndexBuffer(const uint32* InIndices, uint32 MaxIndexCount)
{
	assert(RenderDevice);
	return RenderDevice->CreateStaticIndexBuffer(InIndices, MaxIndexCount);
}

TUniquePtr<FRHIIndexBuffer> FRenderCommand::CreateDynamicIndexBuffer(uint32 MaxIndexCount)
{
	assert(RenderDevice);
	return RenderDevice->CreateDynamicIndexBuffer(MaxIndexCount);
}

TUniquePtr<FRHIUniformBuffer> FRenderCommand::CreateConstantBuffer(uint32 BufferSize)
{
	assert(RenderDevice);
	return RenderDevice->CreateConstantBuffer(BufferSize);
}

TUniquePtr<FRHITexture2D> FRenderCommand::CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const FImageData& Image)
{
	assert(RenderDevice);
	return RenderDevice->CreateTexture2D(Desc, Image);
}

TUniquePtr<FRHITexture2D> FRenderCommand::CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const void* InitialData)
{
	assert(RenderDevice);
	return RenderDevice->CreateTexture2D(Desc, InitialData);
}

TUniquePtr<FRHITextureCube> FRenderCommand::CreateTextureCube(const D3D11_TEXTURE2D_DESC& Desc, const TArray<const void*>& InitialDatas)
{
	assert(RenderDevice);
	return RenderDevice->CreateTextureCube(Desc, InitialDatas);
}

TUniquePtr<FRHIVertexShader> FRenderCommand::CreateVertexShader(const FShaderByteCode& ByteCode)
{
	assert(RenderDevice);
	return RenderDevice->CreateVertexShader(ByteCode);
}

TUniquePtr<FRHIPixelShader> FRenderCommand::CreatePixelShader(const FShaderByteCode& ByteCode)
{
	assert(RenderDevice);
	return RenderDevice->CreatePixelShader(ByteCode);
}

TUniquePtr<FRHIComputeShader> FRenderCommand::CreateComputeShader(const FShaderByteCode& ByteCode)
{
	assert(RenderDevice);
	return RenderDevice->CreateComputeShader(ByteCode);
}

ComPtr<ID3D11DeviceContext> FRenderCommand::CreateDeferredContext()
{
	assert(RenderDevice);
	return RenderDevice->CreateDeferredContext();
}

void FRenderCommand::ExecuteCommandList(ID3D11CommandList* CommandList, bool bRestoreState)
{
	assert(RenderDevice);
	RenderDevice->GetContext()->ExecuteCommandList(CommandList, bRestoreState ? TRUE : FALSE);
}

void FRenderCommand::BindPipelineState(const FPipelineState* PipelineState, ID3D11DeviceContext* Context)
{
	if (!PipelineState)
	{
		return;
	}

	BindShaderProgram(PipelineState->Shader, Context);
	SetPrimitiveTopology(PipelineState->Topology, Context);
	SetRasterizerState(PipelineState->RasterizerState, Context);
	SetBlendState(PipelineState->BlendState, Context);
	SetDepthStencilState(PipelineState->DepthStencilState, Context);
}

void FRenderCommand::BindShaderProgram(FShaderProgram* Shader, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	Ctx->VSSetShader(Shader->VertexShader->GetShader(), nullptr, 0);
	Ctx->PSSetShader(Shader->PixelShader->GetShader(), nullptr, 0);
	Ctx->IASetInputLayout(Shader->VertexShader->GetLayout());
}

void FRenderCommand::BindMesh(UStaticMesh* Mesh, uint8 LODIndex, ID3D11DeviceContext* Context)
{
	BindVertexBuffer(Mesh->VertexBuffer.get(), Context);
	BindIndexBuffer(Mesh->GetIndexBuffer(LODIndex), Context);
}

void FRenderCommand::Draw(uint32 VertexCount, uint32 StartIndexLocation, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	Ctx->Draw(VertexCount, StartIndexLocation);
}

void FRenderCommand::DrawIndexed(uint32 IndexCount, uint32 StartIndexLocation, int32 BaseVertexLocation, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	Ctx->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
}

void FRenderCommand::DrawInstance(uint32 IndexCount, uint32 StartIndexLocation, int32 BaseVertexLocation, ID3D11DeviceContext* Context)
{
	// 인스턴싱 드로우
}

void* FRenderCommand::MapBufferWriteDiscard(FRHIBuffer* Buffer, ID3D11DeviceContext* Context)
{
	if (!Buffer || !Buffer->GetBuffer())
	{
		if (!Context || Context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
		{
			LOG(Warning, "Cannot map an invalid buffer.");
		}
		return nullptr;
	}
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	D3D11_MAPPED_SUBRESOURCE Mapped{};
	HRESULT hr = Ctx->Map(Buffer->GetBuffer(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
	if (FAILED(hr))
	{
		if (Ctx->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE)
		{
			LOG(Warning, "Failed to map buffer: 0x{:08X}", static_cast<uint32>(hr));
		}
		return nullptr;
	}
	return Mapped.pData;
}

void FRenderCommand::UnmapBuffer(FRHIBuffer* Buffer, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	Ctx->Unmap(Buffer->GetBuffer(), 0);
}

void FRenderCommand::UpdateBufferData(FRHIBuffer* InBuffer, const void* Data, uint32 DataSize, ID3D11DeviceContext* Context)
{
	if (!InBuffer || !Data || DataSize > InBuffer->GetBufferSize())
	{
		LOG(Warning, "Invalid buffer update or insufficient capacity.");
		return;
	}
	void* MappedData = MapBufferWriteDiscard(InBuffer, Context);
	if (!MappedData)
	{
		return;
	}
	std::memcpy(MappedData, Data, DataSize);
	UnmapBuffer(InBuffer, Context);
}

void FRenderCommand::BindVertexBuffer(FRHIVertexBuffer* VertexBuffer, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	ID3D11Buffer* Buffer = VertexBuffer ? VertexBuffer->GetBuffer() : nullptr;

	uint32 Offset = 0;
	uint32 Stride = VertexBuffer ? VertexBuffer->GetStride() : 0;
	Ctx->IASetVertexBuffers(0, 1, &Buffer, &Stride, &Offset);
}

void FRenderCommand::BindIndexBuffer(FRHIIndexBuffer* IndexBuffer, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	ID3D11Buffer* Buffer = IndexBuffer ? IndexBuffer->GetBuffer() : nullptr;

	uint32 Offset = 0;
	Ctx->IASetIndexBuffer(Buffer, DXGI_FORMAT_R32_UINT, Offset);
}

void FRenderCommand::BindConstantBuffer(uint32 Slot, FRHIUniformBuffer* ConstantBuffer, EShaderBindFlagBits FlagBits, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	ID3D11Buffer* Buffer = ConstantBuffer->GetBuffer();
	if (HasFlag(FlagBits, EShaderBindFlagBits::Vertex))
		Ctx->VSSetConstantBuffers(Slot, 1, &Buffer);
	if (HasFlag(FlagBits, EShaderBindFlagBits::Pixel))
		Ctx->PSSetConstantBuffers(Slot, 1, &Buffer);
}

void FRenderCommand::BindConstantBufferRange(uint32 Slot, FRHIUniformBuffer* ConstantBuffer, EShaderBindFlagBits FlagBits, uint32 FirstConstant, uint32 NumConstants, ID3D11DeviceContext1* Context)
{
	ID3D11DeviceContext1* Ctx1 = Context ? Context : RenderDevice->GetContext1();

	assert(Ctx1);
	assert(ConstantBuffer);

	ID3D11Buffer* Buffer = ConstantBuffer->GetBuffer();

	if (HasFlag(FlagBits, EShaderBindFlagBits::Vertex))
	{
		Ctx1->VSSetConstantBuffers1(Slot, 1, &Buffer, &FirstConstant, &NumConstants);
	}

	if (HasFlag(FlagBits, EShaderBindFlagBits::Pixel))
	{
		Ctx1->PSSetConstantBuffers1(Slot, 1, &Buffer, &FirstConstant, &NumConstants);
	}
}

void FRenderCommand::BindShaderResource(uint32 Slot, FRHITexture2D* Texture2D, EShaderBindFlagBits FlagBits, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	ID3D11ShaderResourceView* SRV = Texture2D->GetSRV();
	if (HasFlag(FlagBits, EShaderBindFlagBits::Vertex))
	{
		Ctx->VSSetShaderResources(Slot, 1, &SRV);
	}

	if (HasFlag(FlagBits, EShaderBindFlagBits::Pixel))
	{
		Ctx->PSSetShaderResources(Slot, 1, &SRV);
	}
}

void FRenderCommand::BindShaderResource(uint32 Slot, UTexture2D* Texture2D, EShaderBindFlagBits FlagBits, ID3D11DeviceContext* Context)
{
	BindShaderResource(Slot, Texture2D->GetResource(), FlagBits, Context);
}

void FRenderCommand::BeginRenderPass(const FRenderingInfo& RenderingInfo)
{
	TArray<ID3D11RenderTargetView*> RTVs;
	for (const FRenderingDesc& RenderTargetDesc : RenderingInfo.ColorRenderTargets)
	{
		FClearValue ClearValue = RenderTargetDesc.ClearValue;
		if (RenderTargetDesc.LoadOp == ERenderTargetLoadOp::Clear)
		{
			RenderDevice->GetContext()->ClearRenderTargetView(RenderTargetDesc.Texture->GetRTV(), &RenderTargetDesc.ClearValue.ColorClearValue.V[0]);
		}
		RTVs.Add(RenderTargetDesc.Texture->GetRTV());
	}

	ID3D11DepthStencilView* DSV = nullptr;
	if (RenderingInfo.DepthSteincil.Texture != nullptr)
	{
		FClearValue dsvClearValue = RenderingInfo.DepthSteincil.ClearValue;
		DSV = RenderingInfo.DepthSteincil.Texture->GetDSV();
		if (RenderingInfo.DepthSteincil.LoadOp == ERenderTargetLoadOp::Clear)
		{
			RenderDevice->GetContext()->ClearDepthStencilView(RenderingInfo.DepthSteincil.Texture->GetDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, dsvClearValue.depthClearValue, dsvClearValue.stencilClearValue);
		}
	}
	RenderDevice->GetContext()->OMSetRenderTargets((uint32)RTVs.Num(), RTVs.GetData(), DSV);


	SetViewport(RenderingInfo.ViewportSetting.StartX,
		RenderingInfo.ViewportSetting.StartY,
		RenderingInfo.ViewportSetting.Width,
		RenderingInfo.ViewportSetting.Height);
}

void FRenderCommand::EndRenderPass(const FRenderingInfo& RenderingInfo)
{
	RenderDevice->GetContext()->OMSetRenderTargets(0, nullptr, nullptr);
}

void FRenderCommand::ClearDepthStencil(FRHITexture2D* DepthStencilTexture, float Depth, uint8 Stencil)
{
	if (DepthStencilTexture == nullptr)
	{
		return;
	}

	RenderDevice->GetContext()->ClearDepthStencilView(DepthStencilTexture->GetDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, Depth, Stencil);
}

void FRenderCommand::SetViewport(uint32 InX, uint32 InY, uint32 InWidth, uint32 InHeight, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	D3D11_VIEWPORT Viewport;
	Viewport.TopLeftX = (float)InX;
	Viewport.TopLeftY = (float)InY;
	Viewport.Width = (float)InWidth;
	Viewport.Height = (float)InHeight;
	Viewport.MinDepth = 0.0f;
	Viewport.MaxDepth = 1.0f;

	Ctx->RSSetViewports(1, &Viewport);
}

void FRenderCommand::SetRasterizerState(ERasterizerState State, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	Ctx->RSSetState(RenderDevice->GetRasterizerState(State));
}

void FRenderCommand::SetBlendState(EBlendState State, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	Ctx->OMSetBlendState(RenderDevice->GetBlendState(State), nullptr, 0xffffffff);
}

void FRenderCommand::SetDepthStencilState(EDepthStencilState State, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	Ctx->OMSetDepthStencilState(RenderDevice->GetDepthStencilState(State), 0);
}

void FRenderCommand::BindSamplerState(uint32 Slot, ESamplerState SamplerState, EShaderBindFlagBits FlagBits, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	ID3D11SamplerState* Sampler = RenderDevice->GetSamplerState(SamplerState);
	if (HasFlag(FlagBits, EShaderBindFlagBits::Vertex))
	{
		Ctx->VSSetSamplers(Slot, 1, &Sampler);
	}

	if (HasFlag(FlagBits, EShaderBindFlagBits::Pixel))
	{
		Ctx->PSSetSamplers(Slot, 1, &Sampler);
	}

	if (HasFlag(FlagBits, EShaderBindFlagBits::Compute))
	{
		Ctx->CSSetSamplers(Slot, 1, &Sampler);
	}
}

void FRenderCommand::CSSetSampler(uint32 Slot, ESamplerState SamplerState, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	ID3D11SamplerState* Sampler = RenderDevice->GetSamplerState(SamplerState);
	Ctx->CSSetSamplers(Slot, 1, &Sampler);
}

void FRenderCommand::Dispatch(uint32 X, uint32 Y, uint32 Z, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	Ctx->Dispatch(X, Y, Z);
}

void FRenderCommand::CSSetShader(FRHIComputeShader* Shader, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	Ctx->CSSetShader(Shader ? Shader->GetShader() : nullptr, nullptr, 0);
}

void FRenderCommand::CSSetShaderResource(uint32 Slot, ID3D11ShaderResourceView* SRV, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	Ctx->CSSetShaderResources(Slot, 1, &SRV);
}

void FRenderCommand::CSSetShaderResources(uint32 StartSlot, uint32 NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	Ctx->CSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

void FRenderCommand::CSSetUnorderedAccessView(uint32 Slot, ID3D11UnorderedAccessView* UAV, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	uint32 InitialCount = 0xffffffff;
	Ctx->CSSetUnorderedAccessViews(Slot, 1, &UAV, &InitialCount);
}

void FRenderCommand::CSSetUnorderedAccessViews(uint32 StartSlot, uint32 NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const uint32* pUAVInitialCounts, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	Ctx->CSSetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}

void FRenderCommand::CSSetConstantBuffer(uint32 Slot, FRHIUniformBuffer* ConstantBuffer, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	ID3D11Buffer* Buffer = ConstantBuffer ? ConstantBuffer->GetBuffer() : nullptr;
	Ctx->CSSetConstantBuffers(Slot, 1, &Buffer);
}

void FRenderCommand::CopyResource(ID3D11Resource* Dst, ID3D11Resource* Src, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	Ctx->CopyResource(Dst, Src);
}
void FRenderCommand::PSSetShaderResource(uint32 Slot, ID3D11ShaderResourceView* SRV, ID3D11DeviceContext* Context)
{
	ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
	Ctx->CSSetShaderResources(Slot, 1, &SRV);
}

