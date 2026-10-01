#pragma once

#include "RenderDevice.h"

class FBuffer;
class UStaticMesh;
class UTexture2D;
class FShaderProgram;
class FVertexShader;
class FPixelShader;
class FComputeShader;
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

class RenderCommand
{
public:
	static void Init(FRenderDevice* InRenderDevice);

	static TUniquePtr<FVertexBuffer> CreateStaticVertexBuffer(const void* InVertices, uint32 InSize, uint32 Stride);
	static TUniquePtr<FVertexBuffer> CreateDynamicVertexBuffer(uint32 MaxSize, uint32 Stride);

	static TUniquePtr<FIndexBuffer> CreateStaticIndexBuffer(const uint32* InIndices, uint32 MaxIndexCount);
	static TUniquePtr<FIndexBuffer> CreateDynamicIndexBuffer(uint32 MaxIndexCount);

	static TUniquePtr<FConstantBuffer> CreateConstantBuffer(uint32 BufferSize);

	static TUniquePtr<FTexture2D> CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const void* InitialData = nullptr);
	static TUniquePtr<FTexture2D> CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const FImageData& Image);
	static TUniquePtr<FTextureCube> CreateTextureCube(const D3D11_TEXTURE2D_DESC& Desc, const TArray<const void*>& InitialDatas);

	static TUniquePtr<FVertexShader> CreateVertexShader(const FShaderByteCode& ByteCode);
	static TUniquePtr<FPixelShader> CreatePixelShader(const FShaderByteCode& ByteCode);
	static TUniquePtr<FComputeShader> CreateComputeShader(const FShaderByteCode& ByteCode);

	static ComPtr<ID3D11DeviceContext> CreateDeferredContext();
	static void ExecuteCommandList(ID3D11CommandList* CommandList, bool bRestoreState = false);
	static FRenderDevice* GetRenderDevice() { return RenderDevice; }
	static ID3D11Device* GetDevice() { return RenderDevice ? RenderDevice->GetDevice() : nullptr; }
	static ID3D11DeviceContext* GetContext() { return RenderDevice ? RenderDevice->GetContext() : nullptr; }

	static void Dispatch(uint32 X, uint32 Y, uint32 Z, ID3D11DeviceContext* Context = nullptr);
	static void CSSetShader(FComputeShader* Shader, ID3D11DeviceContext* Context = nullptr);
	static void CSSetShaderResource(uint32 Slot, ID3D11ShaderResourceView* SRV, ID3D11DeviceContext* Context = nullptr);
	static void CSSetShaderResources(uint32 StartSlot, uint32 NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews, ID3D11DeviceContext* Context = nullptr);
	static void CSSetUnorderedAccessView(uint32 Slot, ID3D11UnorderedAccessView* UAV, ID3D11DeviceContext* Context = nullptr);
	static void CSSetUnorderedAccessViews(uint32 StartSlot, uint32 NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const uint32* pUAVInitialCounts = nullptr, ID3D11DeviceContext* Context = nullptr);
	static void CSSetConstantBuffer(uint32 Slot, FConstantBuffer* ConstantBuffer, ID3D11DeviceContext* Context = nullptr);
	static void CSSetSampler(uint32 Slot, ESamplerState SamplerState, ID3D11DeviceContext* Context = nullptr);
	static void CopyResource(ID3D11Resource* Dst, ID3D11Resource* Src, ID3D11DeviceContext* Context = nullptr);

	static void BindPipelineState(const FPipelineState* PipelineState, ID3D11DeviceContext* Context = nullptr);

	static void BindMesh(UStaticMesh* Mesh, uint8 LODIndex = 0, ID3D11DeviceContext* Context = nullptr);

	static void BindShaderProgram(FShaderProgram* Shader, ID3D11DeviceContext* Context = nullptr);

	static void Draw(uint32 VertexCount, uint32 StartIndexLocation = 0, ID3D11DeviceContext* Context = nullptr);
	static void DrawIndexed(uint32 IndexCount, uint32 StartIndexLocation = 0, int32 BaseVertexLocation = 0, ID3D11DeviceContext* Context = nullptr);
	static void DrawInstance(uint32 IndexCount, uint32 StartIndexLocation = 0, int32 BaseVertexLocation = 0, ID3D11DeviceContext* Context = nullptr);

	static void* MapBufferWriteDiscard(FBuffer* Buffer, ID3D11DeviceContext* Context = nullptr);
	static void UnmapBuffer(FBuffer* Buffer, ID3D11DeviceContext* Context = nullptr);

	template <typename T>
	static void UpdateBufferData(FBuffer* InBuffer, T* Data, ID3D11DeviceContext* Context = nullptr)
	{
		UpdateBufferData(InBuffer, Data, sizeof(T), Context);
	}
	static void UpdateBufferData(FBuffer* InBuffer, const void* Data, uint32 DataSize, ID3D11DeviceContext* Context = nullptr);

	static void BindVertexBuffer(FVertexBuffer* VertexBuffer, ID3D11DeviceContext* Context = nullptr);
	static void BindIndexBuffer(FIndexBuffer* IndexBuffer, ID3D11DeviceContext* Context = nullptr);
	static void BindConstantBuffer(uint32 Slot, FConstantBuffer* ConstantBuffer, EShaderBindFlagBits FlagBits, ID3D11DeviceContext* Context = nullptr);
	static void BindConstantBufferRange(uint32 Slot, FConstantBuffer* ConstantBuffer, EShaderBindFlagBits FlagBits, uint32 FirstConstant, uint32 NumConstants, ID3D11DeviceContext1* Context = nullptr);
	static void BindShaderResource(uint32 Slot, FTexture2D* Texture2D, EShaderBindFlagBits FlagBits, ID3D11DeviceContext* Context = nullptr);
	static void BindShaderResource(uint32 Slot, UTexture2D* Texture2D, EShaderBindFlagBits FlagBits, ID3D11DeviceContext* Context = nullptr);

	static void BeginRenderPass(const FRenderingInfo& RenderingInfo);
	static void EndRenderPass(const FRenderingInfo& RenderingInfo);
	static void ClearDepthStencil(FTexture2D* DepthStencilTexture, float Depth = 1.0f, uint8 Stencil = 0);

	static void SetViewport(uint32 InX, uint32 InY, uint32 InWidth, uint32 InHeight, ID3D11DeviceContext* Context = nullptr);

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

	//inline static void BindShader(FShader* InShader);

	//inline static void BindMesh(UStaticMesh* InMesh);


private:
	inline static FRenderDevice* RenderDevice = nullptr;
};