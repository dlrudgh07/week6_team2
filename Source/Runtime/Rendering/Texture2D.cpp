#include "EnginePCH.h"
#include "Texture2D.h"

#include "RenderCommand.h"


FTexture2D::FTexture2D(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const FImageData& Image)
	: FTexture2D(Device, InDesc, Image.IsValid() ? Image.Pixels.GetData() : nullptr, Image.GetRowPitch())
{
}

FTexture2D::FTexture2D(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc, const void* InitialData, uint32 RowPitch)
{
	Width = InDesc.Width;
	Height = InDesc.Height;
	Depth = 1;
	Format = InDesc.Format;
	Dimension = ETextureDimension::Texture2D;

	D3D11_SUBRESOURCE_DATA SubData{};
	if (InitialData)
	{
		if (RowPitch == 0)
		{
			const uint32 BytesPerPixel = FormatToBytes(InDesc.Format);
			if (BytesPerPixel == 0)
			{
				LOG(Error, "[Texture2D] 픽셀당 바이트를 알 수 없는 포맷({})입니다. RowPitch를 직접 넘기세요.", (uint32)InDesc.Format);
				return;
			}
			RowPitch = InDesc.Width * BytesPerPixel;
		}

		SubData.pSysMem = InitialData;
		SubData.SysMemPitch = RowPitch;
	}

	HRESULT hr = Device->CreateTexture2D(&InDesc, InitialData ? &SubData : nullptr, (ID3D11Texture2D**)Texture.GetAddressOf());
	if (FAILED(hr))
	{
		LOG(Error, "[Texture2D] CreateTexture2D failed (hr=0x{:08X}, {}x{})", (uint32)hr, InDesc.Width, InDesc.Height);
		return;
	}

	CreateViews(Device, InDesc);
}
FTexture2D::FTexture2D(ID3D11Device* Device, ComPtr<ID3D11Resource> SwapchainTexture, const D3D11_TEXTURE2D_DESC& InDesc)
{
	Texture = std::move(SwapchainTexture);

	Width = InDesc.Width;
	Height = InDesc.Height;
	Depth = 1;
	Format = InDesc.Format;
	Dimension = ETextureDimension::Texture2D;

	CreateViews(Device, InDesc);
}

void FTexture2D::CreateViews(ID3D11Device* Device, const D3D11_TEXTURE2D_DESC& InDesc)
{
	HRESULT hr;
	if (InDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		if (InDesc.Format == DXGI_FORMAT_R24G8_TYPELESS)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
			SRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			SRVDesc.Texture2D.MipLevels = InDesc.MipLevels ? InDesc.MipLevels : 1;
			hr = Device->CreateShaderResourceView(Texture.Get(), &SRVDesc, SRV.GetAddressOf());
		}
		else
		{
			hr = Device->CreateShaderResourceView(Texture.Get(), nullptr, SRV.GetAddressOf());
		}
		if (FAILED(hr))
			LOG(Error, "[Texture2D] CreateShaderResourceView failed (hr=0x{:08X})", (uint32)hr);
	}

	if (InDesc.BindFlags & D3D11_BIND_RENDER_TARGET)
	{
		hr = Device->CreateRenderTargetView(Texture.Get(), nullptr, RTV.GetAddressOf());
		if (FAILED(hr))
			LOG(Error, "[Texture2D] CreateRenderTargetView failed (hr=0x{:08X})", (uint32)hr);
	}

	if (InDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
	{
		if (InDesc.Format == DXGI_FORMAT_R24G8_TYPELESS)
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc{};
			DSVDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			hr = Device->CreateDepthStencilView(Texture.Get(), &DSVDesc, DSV.GetAddressOf());
		}
		else
		{
			hr = Device->CreateDepthStencilView(Texture.Get(), nullptr, DSV.GetAddressOf());
		}
		if (FAILED(hr))
			LOG(Error, "[Texture2D] CreateDepthStencilView failed (hr=0x{:08X})", (uint32)hr);
	}
}

UTexture2D::UTexture2D()
{

}
UTexture2D::~UTexture2D()
{
}

