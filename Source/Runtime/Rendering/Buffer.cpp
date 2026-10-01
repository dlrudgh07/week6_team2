#include "EnginePCH.h"
#include "Buffer.h"

FBuffer::FBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc, const void* InitialData)
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

FConstantBuffer::FConstantBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc)
	:FBuffer(Device, Desc)
{
}

FVertexBuffer::FVertexBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc, uint32 InStride, const void* InitialData)
	:FBuffer(Device, Desc, InitialData)
{
	Stride = InStride;
}

FIndexBuffer::FIndexBuffer(ID3D11Device* Device, const D3D11_BUFFER_DESC& Desc, uint32 Count, const void* InitialData)
	:FBuffer(Device, Desc, InitialData)
{
	IndexCount = Count;
}
