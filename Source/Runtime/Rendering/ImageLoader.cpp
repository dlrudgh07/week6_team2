#include "EnginePCH.h"
#include "ImageLoader.h"

#define STB_IMAGE_IMPLEMENTATION   // 이미 다른 곳에 있으면 여기선 빼세요
#include "stb_image.h"

//int Width, Height, Channels;
//float* Pixels = stbi_loadf(Path.c_str(), &Width, &Height, &Channels, 4);
//if (!Pixels) { LOG(Error, "[HDR] load failed: {}", Path); return nullptr; }
//
//D3D11_TEXTURE2D_DESC Desc{};
//Desc.Width = Width;
//Desc.Height = Height;
//Desc.MipLevels = 1;  
//Desc.ArraySize = 1;
//Desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
//Desc.SampleDesc.Count = 1;
//Desc.Usage = D3D11_USAGE_DEFAULT;
//Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
//
//PanoramaTexture = RenderCommand::CreateTexture2D(Desc, Pixels);
//stbi_image_free(Pixels);

FImageData ImageLoader::Load(const FString& Path)
{
	FImageData Result;

	int Width = 0, Height = 0, Channels = 0;
	unsigned char* Pixels = stbi_load(Path.c_str(), &Width, &Height, &Channels, 4);
	if (!Pixels)
	{
		OutputDebugStringA(("[Texture] stbi_load failed: " + Path + " (" + stbi_failure_reason() + ")\n").c_str());
		return FImageData();
	}

	const size_t ByteCount = static_cast<size_t>(Width) * Height * 4;
	Result.Pixels.SetNum(static_cast<int32>(ByteCount));
	memcpy(Result.Pixels.GetData(), Pixels, ByteCount);

	stbi_image_free(Pixels);

	Result.Width = static_cast<uint32>(Width);
	Result.Height = static_cast<uint32>(Height);
	Result.BytesPerPixel = 4;
	Result.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	return Result;
}

FImageData ImageLoader::LoadHDR(const FString& Path)
{
	FImageData Result;

	stbi_set_flip_vertically_on_load(false);
	int Width = 0, Height = 0, Channels = 0;
	float* Pixels = stbi_loadf(Path.c_str(), &Width, &Height, &Channels, 4);
	if (!Pixels)
	{
		LOG(Error, "[Image] HDR load failed: {} ({})", Path, stbi_failure_reason());
		return Result;
	}

	const size_t ByteCount = static_cast<size_t>(Width) * Height * 4 * sizeof(float);
	Result.Pixels.SetNum(static_cast<int32>(ByteCount));
	memcpy(Result.Pixels.GetData(), Pixels, ByteCount);
	stbi_image_free(Pixels);

	Result.Width = static_cast<uint32>(Width);
	Result.Height = static_cast<uint32>(Height);
	Result.BytesPerPixel = 16;
	Result.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	return Result;
}

FImageData ImageLoader::LoadAuto(const FString& Path)
{
	const fs::path Ext = fs::path(Path).extension();
	if (Ext == ".hdr" || Ext == ".HDR") return LoadHDR(Path);
	return Load(Path);
}