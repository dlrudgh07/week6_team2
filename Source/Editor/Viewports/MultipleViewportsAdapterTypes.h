#pragma once
#include "Editor/Viewports/MultipleViewports.h"

// Quad View 사이에서 렌더링과 입력이 비워지는 Splitter gutter의 화면 픽셀 폭이다.
inline constexpr float SplitterThickness = 6.0f;

// View별 UI의 투영·축 정렬 프리셋이며 기존 콤보박스 순서를 유지한다.
enum class EMultipleViewportsCameraPreset
{
    Perspective, OrthographicView, Top, Bottom, Front, Back, Left, Right
};

// GridRenderer가 소유하는 기존 평면 enum을 재정의 없이 참조한다.
enum class EGridPlane : int32;
