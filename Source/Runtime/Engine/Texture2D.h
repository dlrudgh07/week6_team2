#pragma once

#include "Engine/RenderAsset.h"
#include "RHI/RHITexture2D.h"

class UTexture2D : public URenderAsset
{
	DECLARE_CLASS(UTexture2D, URenderAsset)

	friend class UAssetManager;
public:
	UTexture2D();
	~UTexture2D();

	uint32 GetWidth()  const { return Texture ? Texture->GetWidth() : 0; }
	uint32 GetHeight() const { return Texture ? Texture->GetHeight() : 0; }

	FRHITexture2D* GetResource() const { return Texture.get(); }
private:
	void SetResource(TUniquePtr<FRHITexture2D> InResource) { Texture = std::move(InResource); }

	TUniquePtr<FRHITexture2D> Texture;

};

