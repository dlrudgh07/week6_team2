// .obj/.mtl 파일을 불러오는 정적 클래스
// 추후 관련된 경로 기준을 '엔진 프로젝트 루트' -> '게임 프로젝트 루트'로 변경해야 함
#pragma once

#include "ObjInfo.h"
#include "ObjImportSettings.h"
#include "Core/Types.h"
#include "Core/EngineString.h"

struct FStaticMeshData;

class FObjImporter
{
public:
	static TUniquePtr<FStaticMeshData> LoadStaticMeshData(const FString& Path, EObjAxisPreset& Preset);

	// 디버그용 출력
	static void PrintObjInfo(const FObjInfo& ObjInfo);
	static void PrintSMD(const FStaticMeshData& SMD);

private:
	// .obj 파일 경로 -> FObjInfo 변환
	static bool ParseObj(const FString& Path, FObjInfo& Out);

	// FObjInfo -> FStaticMeshData 변환
	static bool Cook(const FObjInfo& Raw, FStaticMeshData& Out, EObjAxisPreset Preset);
};
