#include "EnginePCH.h"

// Source/Editor/main.cpp  — exe. 여기서만 구체 타입을 안다.
#include "Core/EntryPoint.h"

#include "Editor/Application/EditorApplication.h"
#include "Programs/ObjViewer/ObjViewerApp.h"

// 하이브리드 노트북(내장+외장 GPU)에서 드라이버에게 "이 프로그램은 외장 GPU로 돌려라"고 알린다.
// 드라이버는 exe의 export 테이블을 검사하므로 반드시 exe에 들어가는 이 파일에 둔다.
// (정적 라이브러리인 HitoriEngine에 두면 참조가 없어 링커가 버릴 수 있다)
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 1;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

TUniquePtr<FApplication> CreateApplication()
{
#ifdef OBJ_VIEWER
	return MakeUnique<FObjViewerApp>();
#else
	return MakeUnique<FEditorApplication>();
#endif
}