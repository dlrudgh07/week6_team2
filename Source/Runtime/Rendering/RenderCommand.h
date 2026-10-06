#pragma once

#include "RHI/DynamicRHI.h"

class FRHIBuffer;
class UStaticMesh;
class UTexture2D;
class FShaderProgram;
class FRHIVertexShader;
class FRHIPixelShader;
class FRHIComputeShader;
struct FRenderingInfo;

enum class EShaderBindFlagBits : uint32
{
	None = 0,
	Vertex = 1,
	Geometry = 1 << 2,
	Domain = 1 << 3,
	Hull = 1 << 4,
	Pixel = 1 << 5,
	Compute = 1 << 6
};

DEFINE_ENUM_OPERATORS(EShaderBindFlagBits);

class FRenderCommand
{
public:
	static void Init(FDynamicRHI* InRenderDevice);

	static TUniquePtr<FRHIVertexBuffer> CreateStaticVertexBuffer(const void* InVertices, uint32 InSize, uint32 Stride);
	static TUniquePtr<FRHIVertexBuffer> CreateDynamicVertexBuffer(uint32 MaxSize, uint32 Stride);

	static TUniquePtr<FRHIIndexBuffer> CreateStaticIndexBuffer(const uint32* InIndices, uint32 MaxIndexCount);
	static TUniquePtr<FRHIIndexBuffer> CreateDynamicIndexBuffer(uint32 MaxIndexCount);

	static TUniquePtr<FRHIUniformBuffer> CreateConstantBuffer(uint32 BufferSize);

	static TUniquePtr<FRHITexture2D> CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const void* InitialData = nullptr);
	static TUniquePtr<FRHITexture2D> CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const FImageData& Image);
	static TUniquePtr<FRHITextureCube> CreateTextureCube(const D3D11_TEXTURE2D_DESC& Desc, const TArray<const void*>& InitialDatas);

	static TUniquePtr<FRHIVertexShader> CreateVertexShader(const FShaderByteCode& ByteCode);
	static TUniquePtr<FRHIPixelShader> CreatePixelShader(const FShaderByteCode& ByteCode);
	static TUniquePtr<FRHIComputeShader> CreateComputeShader(const FShaderByteCode& ByteCode);

	static ComPtr<ID3D11DeviceContext> CreateDeferredContext();
	static void ExecuteCommandList(ID3D11CommandList* CommandList, bool bRestoreState = false);
	static FDynamicRHI* GetRenderDevice() { return RenderDevice; }
	static ID3D11Device* GetDevice() { return RenderDevice ? RenderDevice->GetDevice() : nullptr; }
	static ID3D11DeviceContext* GetContext() { return RenderDevice ? RenderDevice->GetContext() : nullptr; }

	static void Dispatch(uint32 X, uint32 Y, uint32 Z, ID3D11DeviceContext* Context = nullptr);
	static void CSSetShader(FRHIComputeShader* Shader, ID3D11DeviceContext* Context = nullptr);
	static void CSSetShaderResource(uint32 Slot, ID3D11ShaderResourceView* SRV, ID3D11DeviceContext* Context = nullptr);
	static void CSSetShaderResources(uint32 StartSlot, uint32 NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews, ID3D11DeviceContext* Context = nullptr);
	static void CSSetUnorderedAccessView(uint32 Slot, ID3D11UnorderedAccessView* UAV, ID3D11DeviceContext* Context = nullptr);
	static void CSSetUnorderedAccessViews(uint32 StartSlot, uint32 NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const uint32* pUAVInitialCounts = nullptr, ID3D11DeviceContext* Context = nullptr);
	static void CSSetConstantBuffer(uint32 Slot, FRHIUniformBuffer* ConstantBuffer, ID3D11DeviceContext* Context = nullptr);
	static void CSSetSampler(uint32 Slot, ESamplerState SamplerState, ID3D11DeviceContext* Context = nullptr);
	static void CopyResource(ID3D11Resource* Dst, ID3D11Resource* Src, ID3D11DeviceContext* Context = nullptr);
	static void PSSetShaderResource(uint32 Slot, ID3D11ShaderResourceView* SRV, ID3D11DeviceContext* Context = nullptr);

	static void BindPipelineState(const FPipelineState* PipelineState, ID3D11DeviceContext* Context = nullptr);

	static void BindMesh(UStaticMesh* Mesh, uint8 LODIndex = 0, ID3D11DeviceContext* Context = nullptr);

	static void BindShaderProgram(FShaderProgram* Shader, ID3D11DeviceContext* Context = nullptr);

	static void Draw(uint32 VertexCount, uint32 StartIndexLocation = 0, ID3D11DeviceContext* Context = nullptr);
	static void DrawIndexed(uint32 IndexCount, uint32 StartIndexLocation = 0, int32 BaseVertexLocation = 0, ID3D11DeviceContext* Context = nullptr);
	static void DrawInstance(uint32 IndexCount, uint32 StartIndexLocation = 0, int32 BaseVertexLocation = 0, ID3D11DeviceContext* Context = nullptr);

	static void* MapBufferWriteDiscard(FRHIBuffer* Buffer, ID3D11DeviceContext* Context = nullptr);
	static void UnmapBuffer(FRHIBuffer* Buffer, ID3D11DeviceContext* Context = nullptr);

	template <typename T>
	static void UpdateBufferData(FRHIBuffer* InBuffer, T* Data, ID3D11DeviceContext* Context = nullptr)
	{
		UpdateBufferData(InBuffer, Data, sizeof(T), Context);
	}
	static void UpdateBufferData(FRHIBuffer* InBuffer, const void* Data, uint32 DataSize, ID3D11DeviceContext* Context = nullptr);

	static void BindVertexBuffer(FRHIVertexBuffer* VertexBuffer, ID3D11DeviceContext* Context = nullptr);
	static void BindIndexBuffer(FRHIIndexBuffer* IndexBuffer, ID3D11DeviceContext* Context = nullptr);
	static void BindConstantBuffer(uint32 Slot, FRHIUniformBuffer* ConstantBuffer, EShaderBindFlagBits FlagBits, ID3D11DeviceContext* Context = nullptr);
	static void BindConstantBufferRange(uint32 Slot, FRHIUniformBuffer* ConstantBuffer, EShaderBindFlagBits FlagBits, uint32 FirstConstant, uint32 NumConstants, ID3D11DeviceContext1* Context = nullptr);
	static void BindShaderResource(uint32 Slot, FRHITexture2D* Texture2D, EShaderBindFlagBits FlagBits, ID3D11DeviceContext* Context = nullptr);
	static void BindShaderResource(uint32 Slot, UTexture2D* Texture2D, EShaderBindFlagBits FlagBits, ID3D11DeviceContext* Context = nullptr);

	static void BeginRenderPass(const FRenderingInfo& RenderingInfo);
	static void BeginRenderPassSceneColor(const FRenderingInfo& RenderingInfo);
	static void EndRenderPass(const FRenderingInfo& RenderingInfo);
	static void ClearDepthStencil(FRHITexture2D* DepthStencilTexture, float Depth = 1.0f, uint8 Stencil = 0);

	static void SetViewport(uint32 InX, uint32 InY, uint32 InWidth, uint32 InHeight, ID3D11DeviceContext* Context = nullptr);
	static void SetRenderTarget(FRHITexture2D* ColorTarget, FRHITexture2D* DepthStencil = nullptr, ID3D11DeviceContext* Context = nullptr);
	static void SetRasterizerState(ERasterizerState State, ID3D11DeviceContext* Context = nullptr);
	static void SetBlendState(EBlendState State, ID3D11DeviceContext* Context = nullptr);
	static void SetDepthStencilState(EDepthStencilState State, ID3D11DeviceContext* Context = nullptr);
	static void BindSamplerState(uint32 Slot, ESamplerState SamplerState, EShaderBindFlagBits FlagBits, ID3D11DeviceContext* Context = nullptr);


	inline static void SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology, ID3D11DeviceContext* Context = nullptr)
	{
		ID3D11DeviceContext* Ctx = Context ? Context : RenderDevice->GetContext();
		Ctx->IASetPrimitiveTopology(Topology);
	}

	//inline static void BindTexture(uint32 Slot, UTexture2D* Texture2D, EShaderBindFlagBits FlagBits);

	//inline static void SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology);

	//inline static void SetDepthStencilEnabled(bool bEnabled);

	//inline static void SetBlendStateEnabled(bool bEnabled);

	//inline static void BindShader(FRHIShader* InShader);

	//inline static void BindMesh(UStaticMesh* InMesh);


private:
	inline static FDynamicRHI* RenderDevice = nullptr;
};