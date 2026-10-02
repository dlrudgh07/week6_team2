#pragma once
#include <wrl/client.h>
#include "HAL/Platform.h"

#include "RHI/RHIDefinitions.h"
#include "Rendering/RenderUtil.h"


struct FConstantBufferBinding
{
	FString Name;
	uint32  Slot = 0;
	uint32  Size = 0;       // 바이트 크기
};

struct FTextureBinding
{
	FString           Name;
	uint32            Slot = 0;
	D3D_SRV_DIMENSION Dimension = D3D_SRV_DIMENSION_UNKNOWN;   // TEXTURE2D / TEXTURECUBE
};

struct FSamplerBinding
{
	FString Name;
	uint32  Slot = 0;
};

class FShaderBindings
{
public:
	const FConstantBufferBinding* FindConstantBuffer(const FString& Name) const;
	const FTextureBinding* FindTexture(const FString& Name) const;
	const FSamplerBinding* FindSampler(const FString& Name) const;

	TArray<FConstantBufferBinding> ConstantBuffers;
	TArray<FTextureBinding>        Textures;
	TArray<FSamplerBinding>        Samplers;
};

class FRHIShader
{
public:
	virtual ~FRHIShader() = default;         

	EShaderType GetType() const { return Type; }
	const FShaderBindings& GetBindings() const { return Bindings; }

	virtual bool IsValid() const { return false; }

protected:
	void BindingReflection(const FShaderByteCode& ByteCode);

	EShaderType Type;
	FShaderBindings Bindings;
};

class FRHIVertexShader : public FRHIShader
{
public:
	FRHIVertexShader(ID3D11Device* Device, const FShaderByteCode& ByteCode);
	virtual ~FRHIVertexShader() = default;

	ID3D11VertexShader* GetShader() const { return VertexShader.Get(); }
	ID3D11InputLayout* GetLayout() const { return InputLayout.Get(); }

	virtual bool IsValid() const { return VertexShader != nullptr; }

private:
	void InputLayoutReflection(ID3D11Device* Device, const FShaderByteCode& ByteCode);

	ComPtr<ID3D11VertexShader> VertexShader = nullptr;
	ComPtr<ID3D11InputLayout> InputLayout = nullptr;
};

class FRHIPixelShader : public FRHIShader
{
public:
	FRHIPixelShader(ID3D11Device* Device, const FShaderByteCode& ByteCode);
	virtual ~FRHIPixelShader() = default;

	ID3D11PixelShader* GetShader() const { return PixelShader.Get(); }

	virtual bool IsValid() const { return PixelShader != nullptr; }
private:
	ComPtr<ID3D11PixelShader> PixelShader = nullptr;
};

class FRHIComputeShader : public FRHIShader
{
public:
	FRHIComputeShader(ID3D11Device* Device, const FShaderByteCode& ByteCode);
	virtual ~FRHIComputeShader() = default;

	ID3D11ComputeShader* GetShader() const { return ComputeShader.Get(); }
	virtual bool IsValid() const { return ComputeShader != nullptr; }

private:
	ComPtr<ID3D11ComputeShader> ComputeShader = nullptr;
};

class FShaderProgram
{
public:
	FShaderProgram(FRHIVertexShader* InVertexShader, FRHIPixelShader* InPixelShader);

	FRHIVertexShader* VertexShader;
	FRHIPixelShader* PixelShader;
};
