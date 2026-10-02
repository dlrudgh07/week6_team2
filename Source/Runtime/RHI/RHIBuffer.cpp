#include "EnginePCH.h"
#include "RHI/RHIBuffer.h"

FRHIBuffer::FRHIBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc, const void* InitialData)
{
	BufferSize = Desc.ByteWidth;

	HRESULT hr;
	if (InitialData)
	{
		D3D11_SUBRESOURCE_DATA Data;
		Data.pSysMem = InitialData;

		hr = Device->CreateBuffer(&Desc, &Data, Buffer.GetAddressOf());
	}
	else
	{
		hr = Device->CreateBuffer(&Desc, nullptr, Buffer.GetAddressOf());
	}
	if (FAILED(hr))
	{
		LOG(Warning, "Failed To Create Buffer!");
	}
}

FRHIUniformBuffer::FRHIUniformBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc)
	:FRHIBuffer(Device, Desc)
{
}

FRHIVertexBuffer::FRHIVertexBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc, uint32 InStride, const void* InitialData)
	:FRHIBuffer(Device, Desc, InitialData)
{
	Stride = InStride;
}

FRHIIndexBuffer::FRHIIndexBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc, uint32 Count, const void* InitialData)
	:FRHIBuffer(Device, Desc, InitialData)
{
	IndexCount = Count;
}
