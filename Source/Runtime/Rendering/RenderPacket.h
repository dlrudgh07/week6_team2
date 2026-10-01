#pragma once
#include "Core/Types.h"
#include "Math/Matrix.h"

class UStaticMesh;
class UMaterial;

struct FRenderPacket {
  FMatrix model;
  FMatrix MVP;
  UStaticMesh *mesh = nullptr;
  UMaterial *material = nullptr;

  // 카메라와의 거리 제곱. 불투명은 front-to-back, 반투명은 back-to-front 정렬에 사용한다.
  float CameraDistanceSquared = 0.0f;

  const void *MaterialParamData = nullptr;
  uint32 MaterialParamDataSize = 0;

  uint32 StartIndex = 0;
  uint32 IndexCount = 0; // 0이면 전체 IndexBuffer 사용
  uint8 LODIndex = 0;
};
