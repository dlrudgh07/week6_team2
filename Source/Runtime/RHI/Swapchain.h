#pragma once

#include "RHI/DynamicRHI.h"
#include "Rendering/RenderingInfo.h"

class FWindowsWindow;

class FSwapchain
{
public:
public:
	FSwapchain(FDynamicRHI* InRenderDevice, FWindowsWindow* InWindow);
	void CreateBackbuffer();
	~FSwapchain();

	// 스왑체인 크기 변경시
	void Resize(int32 Width, int32 Height);

	void SwapBuffers(uint32 SyncInterval = 1, uint32 Flags = 0);
	bool IsTearingSupported() const { return bAllowTearing; }

	const FRenderingInfo& GetRenderingInfo() const { return RenderingInfo; }
private:
	void ValidateRenderingInfo();
	
	FDynamicRHI* RenderDevice;

	DXGI_SWAP_CHAIN_DESC Desc;
	ComPtr<IDXGISwapChain> Swapchain;
	bool bAllowTearing = false;

	TUniquePtr<FRHITexture2D> BackbufferTexture;

	FRenderingInfo RenderingInfo{};
};
