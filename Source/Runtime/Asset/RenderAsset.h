// 렌더링에 필요한 에셋들의 부모 클래스 (예: 폰트, 텍스쳐, ..)
#pragma once

#include "ObjectSystem/Object.h"
#include "ObjectSystem/Class.h"

class URenderAsset : public UObject
{
	DECLARE_CLASS(URenderAsset, UObject)
public:
	URenderAsset() = default;
	virtual ~URenderAsset() = default;

	void SetPath(FString Path) { AssetPath = Path; }
	const FString& GetPath() const { return AssetPath; }

private:
	FString AssetPath;
};
