#pragma once

#include "RenderDevice.h"
#include "RenderingInfo.h"

class FWindow;

class FSwapchain
{
public:
public:
	FSwapchain(FRenderDevice* InRenderDevice, FWindow* InWindow);
	void CreateBackbuffer();
	~FSwapchain();

	// 스왑체인 크기 변경시
	void Resize(int32 Width, int32 Height);

	void SwapBuffers(uint32 SyncInterval = 1, uint32 Flags = 0);
	bool IsTearingSupported() const { return bAllowTearing; }

	const FRenderingInfo& GetRenderingInfo() const { return RenderingInfo; }
private:
	void ValidateRenderingInfo();
	
	FRenderDevice* RenderDevice;

	DXGI_SWAP_CHAIN_DESC Desc;
	ComPtr<IDXGISwapChain> Swapchain;
	bool bAllowTearing = false;

	TUniquePtr<FTexture2D> BackbufferTexture;

	FRenderingInfo RenderingInfo{};
};
