#include "EnginePCH.h"
#include "Swapchain.h"

#include "Core/Window.h"
#include <dxgi1_5.h>

FSwapchain::FSwapchain(FRenderDevice* InRenderDevice, FWindow* InWindow)
{
	RenderDevice = InRenderDevice;

	DXGI_SAMPLE_DESC SampleDesc{};
	SampleDesc.Count = 1;
	SampleDesc.Quality = 0;

	DXGI_MODE_DESC BufferDesc{};
	BufferDesc.Width = InWindow->GetWidth();
	BufferDesc.Height = InWindow->GetHeight();
	BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	Desc.BufferDesc = BufferDesc;
	Desc.SampleDesc = SampleDesc;
	Desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	Desc.BufferCount = 2;
	Desc.OutputWindow = InWindow->GetHandle();
	Desc.Windowed = true;
	Desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	Desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ComPtr<IDXGIFactory5> Factory5;
	if (SUCCEEDED(RenderDevice->GetFactory()->QueryInterface(IID_PPV_ARGS(&Factory5))))
	{
		BOOL bTearingSupported = FALSE;
		if (SUCCEEDED(Factory5->CheckFeatureSupport(
			DXGI_FEATURE_PRESENT_ALLOW_TEARING,
			&bTearingSupported,
			sizeof(bTearingSupported))))
		{
			bAllowTearing = bTearingSupported == TRUE;
		}
	}
	if (bAllowTearing)
		Desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

	HRESULT hr = RenderDevice->GetFactory()->CreateSwapChain(RenderDevice->GetDevice(), &Desc, Swapchain.GetAddressOf());
	if (FAILED(hr))
	{
		LOG(Error, "Failed To Create Swapchain!");
		return;
	}
	else
		LOG(Info, "Swapchain tearing support: {}", bAllowTearing ? "enabled" : "unavailable");

	CreateBackbuffer();
	if (!BackbufferTexture)
		return;
	ValidateRenderingInfo();
}

void FSwapchain::CreateBackbuffer()
{
	if (!Swapchain)
		return;

	ComPtr<ID3D11Texture2D> Backbuffer;

	const HRESULT Hr = Swapchain->GetBuffer(0, IID_PPV_ARGS(&Backbuffer));

	if (FAILED(Hr) || !Backbuffer)
	{
		LOG(Error, "Failed To Get Swapchain Backbuffer!");
		return;
	}

	D3D11_TEXTURE2D_DESC TextureDesc{};
	Backbuffer->GetDesc(&TextureDesc);

	BackbufferTexture = MakeUnique<FTexture2D>(RenderDevice->GetDevice(), Backbuffer, TextureDesc);
}

FSwapchain::~FSwapchain()
{
}

void FSwapchain::Resize(int32 InWidth, int32 InHeight)
{
	if (!Swapchain || InWidth <= 0 || InHeight <= 0)
		return;

	// ResizeBuffers 전에 backbuffer를 참조하는 모든 출력 바인딩과 View를 해제한다.
	RenderDevice->GetContext()->OMSetRenderTargets(0, nullptr, nullptr);
	BackbufferTexture = nullptr;
	// 생성 시 지정한 플래그(ALLOW_TEARING 등)를 그대로 넘겨야 한다. 0을 넘기면 플래그 불일치로 실패한다.
	const HRESULT Hr = Swapchain->ResizeBuffers(0, InWidth, InHeight, DXGI_FORMAT_UNKNOWN, Desc.Flags);

	if (FAILED(Hr))
	{
		LOG(Error, "Failed To Resize Swapchain! (hr=0x{:08X}, {}x{})",
			static_cast<uint32>(Hr), InWidth, InHeight);

		// 기존 Swapchain의 backbuffer라도 다시 얻어본다.
		CreateBackbuffer();

		if (BackbufferTexture)
			ValidateRenderingInfo();
		return;
	}

	Swapchain->GetDesc(&Desc);
	CreateBackbuffer();

	if (!BackbufferTexture)
		return;

	ValidateRenderingInfo();
}

void FSwapchain::SwapBuffers(uint32 SyncInterval, uint32 Flags)
{
	uint32 PresentFlags = Flags;
	if (SyncInterval == 0 && bAllowTearing)
	{
		BOOL bFullscreen = FALSE;
		if (SUCCEEDED(Swapchain->GetFullscreenState(&bFullscreen, nullptr)) && !bFullscreen)
			PresentFlags |= DXGI_PRESENT_ALLOW_TEARING;
	}
	Swapchain->Present(SyncInterval, PresentFlags);
}

void FSwapchain::ValidateRenderingInfo()
{
	RenderingInfo.ColorRenderTargets.Reset();
	RenderingInfo.ViewportSetting.Width = Desc.BufferDesc.Width;
	RenderingInfo.ViewportSetting.Height = Desc.BufferDesc.Height;

	FRenderingDesc Desc{};
	Desc.Texture = BackbufferTexture.get();

	RenderingInfo.ColorRenderTargets.Add(Desc);
}
