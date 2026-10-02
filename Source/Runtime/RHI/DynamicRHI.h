#pragma once

#include "RHI/RHIStaticStates.h"

class FRHIVertexBuffer;
class FRHIShader;
class FRHIVertexShader;
class FRHIPixelShader;
class FShaderProgram;
struct FShaderByteCode;
struct FImageData;
class FRHIIndexBuffer;
class FRHIUniformBuffer;
class FRHITexture2D;
class FRHITextureCube;
class FPipelineState;
class FRHIComputeShader;

// Device, DeviceContext
class FDynamicRHI
{
public:
	FDynamicRHI();
	~FDynamicRHI();

	ID3D11Device* GetDevice() const { return Device.Get(); }
	ID3D11DeviceContext* GetContext() const { return DeviceContext.Get(); }
	ID3D11DeviceContext1* GetContext1() const {	return DeviceContext1.Get(); }
	IDXGIFactory* GetFactory() const { return DXGIFactory.Get(); }
	bool SupportsConstantBufferOffsetting() const { return bConstantBufferOffsettingSupported; }
	bool SupportsNativeCommandLists() const { return bNativeCommandListsSupported; }

	ComPtr<ID3D11DeviceContext> CreateDeferredContext();

	TUniquePtr<FRHIVertexBuffer> CreateStaticVertexBuffer(const void* InVertices, uint32 InSize, uint32 Stride);
	TUniquePtr<FRHIIndexBuffer> CreateStaticIndexBuffer(const uint32* InIndices, uint32 MaxIndexCount);

	TUniquePtr<FRHIVertexBuffer> CreateDynamicVertexBuffer(uint32 MaxSize, uint32 Stride);
	TUniquePtr<FRHIIndexBuffer> CreateDynamicIndexBuffer(uint32 MaxIndexCount);

	TUniquePtr<FRHIUniformBuffer> CreateConstantBuffer(uint32 BufferSize);

	TUniquePtr<FRHITexture2D> CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const void* InitialData);
	TUniquePtr<FRHITexture2D> CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const FImageData& Image);
	TUniquePtr<FRHITextureCube> CreateTextureCube(const D3D11_TEXTURE2D_DESC& Desc, const TArray<const void*>& InitialDatas);

	TUniquePtr<FRHIVertexShader> CreateVertexShader(const FShaderByteCode& ByteCode);
	TUniquePtr<FRHIPixelShader> CreatePixelShader(const FShaderByteCode& ByteCode);
	TUniquePtr<FRHIComputeShader> CreateComputeShader(const FShaderByteCode& ByteCode);

	TUniquePtr<FShaderProgram> CreateShader(const wchar_t* FileName, D3D11_INPUT_ELEMENT_DESC* InLayoutDesc, size_t InLayoutSize);

	inline ID3D11RasterizerState* GetRasterizerState(ERasterizerState State) const { return RasterizerStates[(uint8)State].Get(); }
	inline ID3D11DepthStencilState* GetDepthStencilState(EDepthStencilState State) const { return DepthStencilStates[(uint8)State].Get(); }
	inline ID3D11BlendState* GetBlendState(EBlendState State) const { return BlendStates[(uint8)State].Get(); }
	inline ID3D11SamplerState* GetSamplerState(ESamplerState State) const { return SamplerStates[(uint8)State].Get(); }

	void Shutdown();
private:
	void CreateStates();

	ComPtr<ID3D11Device> Device;
	ComPtr<ID3D11DeviceContext> DeviceContext;
	ComPtr<ID3D11DeviceContext1> DeviceContext1;
	bool bConstantBufferOffsettingSupported = false;
	bool bNativeCommandListsSupported = false;
	ComPtr<IDXGIFactory> DXGIFactory;

	D3D_FEATURE_LEVEL FeatureLevel;

	ComPtr<ID3D11RasterizerState>   RasterizerStates[(uint8)ERasterizerState::Count];
	ComPtr<ID3D11DepthStencilState> DepthStencilStates[(uint8)EDepthStencilState::Count];
	ComPtr<ID3D11BlendState>        BlendStates[(uint8)EBlendState::Count];
	ComPtr<ID3D11SamplerState>      SamplerStates[(uint8)ESamplerState::Count];
};
