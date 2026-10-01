#include "EnginePCH.h"
#include "RenderDevice.h"

#include "Buffer.h"
#include "Shader.h"
#include "Texture2D.h"
#include "TextureCube.h"
#include "PipelineState.h"

#include <dxgi1_6.h>

namespace
{
	// 하이브리드 노트북에서 고성능(외장) GPU를 명시적으로 고른다.
	// 찾지 못하면 nullptr을 반환하고, 호출부는 기존처럼 기본 어댑터로 생성한다.
	ComPtr<IDXGIAdapter1> FindHighPerformanceAdapter()
	{
		ComPtr<IDXGIFactory6> Factory6;
		if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(Factory6.GetAddressOf()))))
		{
			return nullptr; // Windows 10 1803 이전에는 IDXGIFactory6가 없다
		}

		ComPtr<IDXGIAdapter1> Adapter;
		for (UINT Index = 0;
			SUCCEEDED(Factory6->EnumAdapterByGpuPreference(Index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
				IID_PPV_ARGS(Adapter.ReleaseAndGetAddressOf())));
			++Index)
		{
			DXGI_ADAPTER_DESC1 Desc{};
			Adapter->GetDesc1(&Desc);
			// Microsoft Basic Render Driver 같은 소프트웨어 어댑터는 건너뛴다.
			if (Desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}
			return Adapter;
		}
		return nullptr;
	}

	// 어댑터 이름(UTF-16)을 로그용 UTF-8 문자열로 바꾼다.
	std::string ToUtf8(const wchar_t* Wide)
	{
		const int Size = WideCharToMultiByte(CP_UTF8, 0, Wide, -1, nullptr, 0, nullptr, nullptr);
		if (Size <= 1)
		{
			return {};
		}
		std::string Result(static_cast<size_t>(Size - 1), '\0');
		WideCharToMultiByte(CP_UTF8, 0, Wide, -1, Result.data(), Size, nullptr, nullptr);
		return Result;
	}
}

FRenderDevice::FRenderDevice()
{
	uint32 CreationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
	// If the project is in a debug build, enable debugging via SDK Layers.
	CreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL FeatureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	// 어댑터를 직접 지정할 때는 드라이버 타입이 UNKNOWN이어야 한다. (HARDWARE면 생성 실패)
	ComPtr<IDXGIAdapter1> PreferredAdapter = FindHighPerformanceAdapter();
	D3D_DRIVER_TYPE driverType = PreferredAdapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
	HRESULT hr = D3D11CreateDevice(
		PreferredAdapter.Get(),
		driverType,
		nullptr,
		CreationFlags,
		FeatureLevels,
		ARRAYSIZE(FeatureLevels),
		D3D11_SDK_VERSION,
		Device.GetAddressOf(),
		&FeatureLevel,
		DeviceContext.GetAddressOf());

	if (FAILED(hr))
	{
		LOG(Error, "Failed to Create D3D11Device & DeviceContext!");
	}

	DeviceContext.As(&DeviceContext1);
	D3D11_FEATURE_DATA_D3D11_OPTIONS Options{};
	bConstantBufferOffsettingSupported = DeviceContext1
		&& SUCCEEDED(Device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &Options, sizeof(Options)))
		&& Options.ConstantBufferOffsetting;
	D3D11_FEATURE_DATA_THREADING Threading{};
	bNativeCommandListsSupported = SUCCEEDED(Device->CheckFeatureSupport(D3D11_FEATURE_THREADING, &Threading, sizeof(Threading)))
		&& Threading.DriverCommandLists;

	ComPtr<IDXGIDevice> DXGIDevice;
	Device->QueryInterface(IID_PPV_ARGS(DXGIDevice.GetAddressOf()));
	ComPtr<IDXGIAdapter> DXGIAdapter;
	DXGIDevice->GetParent(IID_PPV_ARGS(DXGIAdapter.GetAddressOf()));
	DXGIAdapter->GetParent(IID_PPV_ARGS(DXGIFactory.GetAddressOf()));

	// 실제로 어떤 GPU에서 돌고 있는지 눈으로 확인한다.
	DXGI_ADAPTER_DESC AdapterDesc{};
	if (SUCCEEDED(DXGIAdapter->GetDesc(&AdapterDesc)))
	{
		LOG(Info, "GPU Adapter: {} (VRAM {} MB)",
			ToUtf8(AdapterDesc.Description),
			static_cast<uint64>(AdapterDesc.DedicatedVideoMemory / (1024 * 1024)));
	}

	CreateStates();
}

FRenderDevice::~FRenderDevice()
{
}

// 지연 컨텍스트 생성
ComPtr<ID3D11DeviceContext> FRenderDevice::CreateDeferredContext()
{
	ComPtr<ID3D11DeviceContext> DeferredContext;
	HRESULT hr = Device->CreateDeferredContext(0, DeferredContext.GetAddressOf());
	if (FAILED(hr))
	{
		LOG(Warning, "Failed to create deferred context: 0x{:08X}", static_cast<uint32>(hr));
		return nullptr;
	}
	return DeferredContext;
}

TUniquePtr<FVertexBuffer> FRenderDevice::CreateStaticVertexBuffer(const void* InVertices, uint32 InSize, uint32 Stride)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_IMMUTABLE;          // 나중에 규모 커지면 immutable
	Desc.ByteWidth = InSize;
	Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	TUniquePtr<FVertexBuffer> Buffer = MakeUnique<FVertexBuffer>(Device.Get(), Desc, Stride, InVertices);

	return Buffer;
}

TUniquePtr<FIndexBuffer> FRenderDevice::CreateStaticIndexBuffer(const uint32* InIndices, uint32 IndexCount)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.ByteWidth = sizeof(uint32) * IndexCount;
	Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	TUniquePtr<FIndexBuffer> Buffer = MakeUnique<FIndexBuffer>(Device.Get(), Desc, IndexCount, InIndices);

	return Buffer;
}

TUniquePtr<FVertexBuffer> FRenderDevice::CreateDynamicVertexBuffer(uint32 MaxSize, uint32 Stride)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.ByteWidth = MaxSize;
	Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	TUniquePtr<FVertexBuffer> Buffer = MakeUnique<FVertexBuffer>(Device.Get(), Desc, Stride);

	return Buffer;
}

TUniquePtr<FIndexBuffer> FRenderDevice::CreateDynamicIndexBuffer(uint32 MaxIndexCount)
{
	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.ByteWidth = sizeof(uint32) * MaxIndexCount;
	Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	TUniquePtr<FIndexBuffer> Buffer = MakeUnique<FIndexBuffer>(Device.Get(), Desc, MaxIndexCount);

	return Buffer;
}

TUniquePtr<FConstantBuffer> FRenderDevice::CreateConstantBuffer(uint32 BufferSize)
{

	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.ByteWidth = BufferSize;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	TUniquePtr<FConstantBuffer> Buffer = MakeUnique<FConstantBuffer>(Device.Get(), Desc);

	return Buffer;
}

TUniquePtr<FTexture2D> FRenderDevice::CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const FImageData& Image)
{
	return MakeUnique<FTexture2D>(Device.Get(), Desc, Image);
}

TUniquePtr<FTexture2D> FRenderDevice::CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const void* InitialData)
{
	TUniquePtr<FTexture2D> Texture = MakeUnique<FTexture2D>(Device.Get(), Desc, InitialData);

	return Texture;
}

TUniquePtr<FTextureCube> FRenderDevice::CreateTextureCube(const D3D11_TEXTURE2D_DESC& Desc, const TArray<const void*>& InitialDatas)
{
	TUniquePtr<FTextureCube> Texture = MakeUnique<FTextureCube>(Device.Get(), Desc, InitialDatas);

	return Texture;
}

TUniquePtr<FVertexShader> FRenderDevice::CreateVertexShader(const FShaderByteCode& ByteCode)
{
	TUniquePtr<FVertexShader> VS = MakeUnique<FVertexShader>(Device.Get(), ByteCode);

	return VS;
}

TUniquePtr<FPixelShader> FRenderDevice::CreatePixelShader(const FShaderByteCode& ByteCode)
{
	TUniquePtr<FPixelShader> PS = MakeUnique<FPixelShader>(Device.Get(), ByteCode);

	return PS;
}

TUniquePtr<FComputeShader> FRenderDevice::CreateComputeShader(const FShaderByteCode& ByteCode)
{
	TUniquePtr<FComputeShader> CS = MakeUnique<FComputeShader>(Device.Get(), ByteCode);

	return CS;
}

//TUniquePtr<FShaderProgram> FRenderDevice::CreateShader(const wchar_t* FileName, D3D11_INPUT_ELEMENT_DESC* InLayoutDesc, size_t InLayoutSize)
//{
	//TUniquePtr<FShaderProgram> Shader = MakeUnique<FShaderProgram>();

	//ID3DBlob* VertexShaderCSO;
	//ID3DBlob* ErrorBlob;
	//HRESULT hr = D3DCompileFromFile(FileName, nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &VertexShaderCSO, &ErrorBlob);

	//if (FAILED(hr))
	//{
	//	if (ErrorBlob)
	//	{
	//		OutputDebugStringA((char*)ErrorBlob->GetBufferPointer());
	//		ErrorBlob->Release();
	//	}
	//	assert(false && "Vertex shader compile failed");
	//	return nullptr; 
	//}

	//hr = Device->CreateVertexShader(VertexShaderCSO->GetBufferPointer(), VertexShaderCSO->GetBufferSize(), nullptr, Shader->VertexShader.GetAddressOf());

	//ID3DBlob* PixelShaderCSO;
	//D3DCompileFromFile(FileName, nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &PixelShaderCSO, nullptr);
	//Device->CreatePixelShader(PixelShaderCSO->GetBufferPointer(), PixelShaderCSO->GetBufferSize(), nullptr, Shader->PixelShader.GetAddressOf());

	//if (InLayoutSize > 0)
	//{
	//	hr = Device->CreateInputLayout(InLayoutDesc, InLayoutSize,
	//		VertexShaderCSO->GetBufferPointer(), VertexShaderCSO->GetBufferSize(), &(Shader->InputLayout));
	//}

	//VertexShaderCSO->Release();
	//PixelShaderCSO->Release();

	//return Shader;
//}

void FRenderDevice::Shutdown()
{
	DeviceContext->ClearState();
	DeviceContext->Flush();
}

// RenderDevice.cpp
void FRenderDevice::CreateStates()
{
	// ---------- Rasterizer ----------
	{
		D3D11_RASTERIZER_DESC Desc = {};
		Desc.FillMode = D3D11_FILL_SOLID;
		Desc.CullMode = D3D11_CULL_BACK;
		Desc.FrontCounterClockwise = FALSE;
		Desc.DepthClipEnable = TRUE;

		Device->CreateRasterizerState(&Desc, RasterizerStates[(uint8)ERasterizerState::SolidBack].GetAddressOf());

		Desc.CullMode = D3D11_CULL_NONE;
		Device->CreateRasterizerState(&Desc, RasterizerStates[(uint8)ERasterizerState::SolidNone].GetAddressOf());

		Desc.CullMode = D3D11_CULL_FRONT;
		Device->CreateRasterizerState(&Desc, RasterizerStates[(uint8)ERasterizerState::SolidFront].GetAddressOf());

		Desc.FillMode = D3D11_FILL_WIREFRAME;
		Desc.CullMode = D3D11_CULL_NONE;
		Device->CreateRasterizerState(&Desc, RasterizerStates[(uint8)ERasterizerState::Wireframe].GetAddressOf());
	}

	// ---------- DepthStencil ----------
	{
		D3D11_DEPTH_STENCIL_DESC Desc = {};
		Desc.DepthEnable = TRUE;
		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		Desc.DepthFunc = D3D11_COMPARISON_LESS;
		Desc.StencilEnable = FALSE;

		Device->CreateDepthStencilState(&Desc, DepthStencilStates[(uint8)EDepthStencilState::Default].GetAddressOf());

		Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		Device->CreateDepthStencilState(&Desc, DepthStencilStates[(uint8)EDepthStencilState::ReadOnly].GetAddressOf());

		Desc.DepthEnable = FALSE;
		Device->CreateDepthStencilState(&Desc, DepthStencilStates[(uint8)EDepthStencilState::Disabled].GetAddressOf());

		// 깊이 없이 선택 메시 영역의 스텐실을 올린다 (참조값 0 기준).
		Desc.StencilEnable = TRUE;
		Desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
		Desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
		Desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		Desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		Desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR_SAT;
		Desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		Desc.BackFace = Desc.FrontFace;
		Device->CreateDepthStencilState(&Desc, DepthStencilStates[(uint8)EDepthStencilState::StencilMask].GetAddressOf());

		// 스텐실이 0인 픽셀(선택 메시 바깥)에만 그린다.
		Desc.StencilWriteMask = 0;
		Desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		Desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
		Desc.BackFace = Desc.FrontFace;
		Device->CreateDepthStencilState(&Desc, DepthStencilStates[(uint8)EDepthStencilState::StencilOutline].GetAddressOf());
	}

	// ---------- Blend ----------
	{
		D3D11_BLEND_DESC Desc = {};
		Desc.RenderTarget[0].BlendEnable = FALSE;
		Desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		Desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
		Desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		Desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		Desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		Device->CreateBlendState(&Desc, BlendStates[(uint8)EBlendState::Opaque].GetAddressOf());

		Desc.RenderTarget[0].BlendEnable = TRUE;
		Desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		Desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;

		Device->CreateBlendState(&Desc, BlendStates[(uint8)EBlendState::AlphaBlend].GetAddressOf());

		// 스텐실 마스크 패스처럼 색은 쓰지 않고 깊이·스텐실만 갱신할 때 쓴다.
		Desc.RenderTarget[0].BlendEnable = FALSE;
		Desc.RenderTarget[0].RenderTargetWriteMask = 0;
		Device->CreateBlendState(&Desc, BlendStates[(uint8)EBlendState::NoColorWrite].GetAddressOf());
	}

	// ---------- Sampler ----------
	{
		D3D11_SAMPLER_DESC Desc = {};
		Desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		Desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		Desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		Desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		Desc.MaxAnisotropy = 1;
		Desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		Desc.MinLOD = 0.0f;
		Desc.MaxLOD = D3D11_FLOAT32_MAX;

		Device->CreateSamplerState(&Desc, SamplerStates[(uint8)ESamplerState::LinearClamp].GetAddressOf());

		Desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		Desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		Desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

		Device->CreateSamplerState(&Desc, SamplerStates[(uint8)ESamplerState::LinearWrap].GetAddressOf());

		Desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		Desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		Desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		Desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		Device->CreateSamplerState(&Desc, SamplerStates[(uint8)ESamplerState::PointClamp].GetAddressOf());
	}
}
