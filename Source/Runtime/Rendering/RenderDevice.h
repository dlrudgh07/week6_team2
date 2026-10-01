#pragma once

#include "RenderStates.h"

class FVertexBuffer;
class FShader;
class FVertexShader;
class FPixelShader;
class FShaderProgram;
struct FShaderByteCode;
struct FImageData;
class FIndexBuffer;
class FConstantBuffer;
class FTexture2D;
class FTextureCube;
class FPipelineState;
class FComputeShader;

// Device, DeviceContext
class FRenderDevice
{
public:
	FRenderDevice();
	~FRenderDevice();

	ID3D11Device* GetDevice() const { return Device.Get(); }
	ID3D11DeviceContext* GetContext() const { return DeviceContext.Get(); }
	ID3D11DeviceContext1* GetContext1() const {	return DeviceContext1.Get(); }
	IDXGIFactory* GetFactory() const { return DXGIFactory.Get(); }
	bool SupportsConstantBufferOffsetting() const { return bConstantBufferOffsettingSupported; }
	bool SupportsNativeCommandLists() const { return bNativeCommandListsSupported; }

	ComPtr<ID3D11DeviceContext> CreateDeferredContext();

	TUniquePtr<FVertexBuffer> CreateStaticVertexBuffer(const void* InVertices, uint32 InSize, uint32 Stride);
	TUniquePtr<FIndexBuffer> CreateStaticIndexBuffer(const uint32* InIndices, uint32 MaxIndexCount);

	TUniquePtr<FVertexBuffer> CreateDynamicVertexBuffer(uint32 MaxSize, uint32 Stride);
	TUniquePtr<FIndexBuffer> CreateDynamicIndexBuffer(uint32 MaxIndexCount);

	TUniquePtr<FConstantBuffer> CreateConstantBuffer(uint32 BufferSize);

	TUniquePtr<FTexture2D> CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const void* InitialData);
	TUniquePtr<FTexture2D> CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const FImageData& Image);
	TUniquePtr<FTextureCube> CreateTextureCube(const D3D11_TEXTURE2D_DESC& Desc, const TArray<const void*>& InitialDatas);

	TUniquePtr<FVertexShader> CreateVertexShader(const FShaderByteCode& ByteCode);
	TUniquePtr<FPixelShader> CreatePixelShader(const FShaderByteCode& ByteCode);
	TUniquePtr<FComputeShader> CreateComputeShader(const FShaderByteCode& ByteCode);

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
