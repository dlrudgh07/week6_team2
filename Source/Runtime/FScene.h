#pragma once
#include "FogSceneData.h"

// World에 배치된 것 중, 화면 결과에 영향을 주고, 모든 뷰에서 똑같은 것. 렌더에 넘기기 위함.
// ex) fog, skybox
struct FScene
{
	FFogSceneData FogSceneData;

};