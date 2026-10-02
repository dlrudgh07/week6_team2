// Cook이 끝난 FStaticMeshData를 .bin으로 저장하고 읽는 클래스
#pragma once

#include "Core/Types.h"
#include "Core/EngineString.h"
#include "Asset/ObjImporter/ObjImportSettings.h"

struct FStaticMeshData;

class FStaticMeshBake
{
public:
	static TUniquePtr<FStaticMeshData> ReadBaked(const FString& BinPath, EObjAxisPreset& OutPreset);
	static void WriteBaked(const FString& BinPath, const FStaticMeshData& Data, EObjAxisPreset Preset);
};
