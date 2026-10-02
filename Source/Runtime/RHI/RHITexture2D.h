#pragma once

#include <d3d11.h>
#include "RHI/DynamicRHI.h"
#include "RHI/RHITexture.h"
#include "Rendering/ImageLoader.h"

class FRHITexture2D : public FRHITexture
{
public:
	// RowPitch가 0이면 InDesc.Format에서 계산한다.
	FRHITexture2D(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const void* InitialData, uint32 RowPitch = 0);
	// 로더가 포맷과 픽치까지 알고 있으므로 그대로 받는다.
	FRHITexture2D(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const FImageData& Image);
	// For Swapchain
	FRHITexture2D(ID3D11Device* Device, ComPtr<ID3D11Resource> SwapchainTexture, const D3D11_TEXTURE2D_DESC& InDesc);
	~FRHITexture2D() = default;

private:
	void CreateViews(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc);
};

