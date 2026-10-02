// .obj 파일의 좌표축 규약을 설정하는 열거형 클래스
#pragma once

#include "HAL/Platform.h"

enum class EObjAxisPreset : uint8
{
	Default,
	ZUp
};