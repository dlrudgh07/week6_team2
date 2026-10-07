#pragma once
#include <cmath>
#include "Vector.h"
#include "Vector2D.h"
#include "Vector4.h"
//#include "TVector.h"
#include "Matrix.h"
#include "Quat.h"
#include "Rotator.h"
#include "Math/Ray.h"
#include "VectorRegister.h"
#include "MatrixRegister.h"

#define PI 3.141592f

namespace FMath
{
	//static float PI = std::acosf(-1);

	static inline bool IsNearlyZero(float Value, double ErrorTolerance = 1e-310f)
	{
		return std::abs(Value) <= ErrorTolerance;
	}

	static inline bool IsNearlyEqual(float Value1, float Value2, float ErrorTolerance = 1e-4f)
	{
		return std::abs(Value1 - Value2) <= ErrorTolerance;
	}

	static inline float Clamp(float Value, float Min, float Max)
	{
		if (Value < Min) return Min;
		if (Value > Max) return Max;
		return Value;
	}

	static inline float RadiansToDegrees(float Radian)
	{
		return Radian * (180.0f / PI);
	}

	static inline float DegreesToRadians(float Degree)
	{
		return Degree * (PI / 180.0f);
	}

	static inline bool Abs(float Value)
	{
		return (Value >= 0) ? Value : -Value;
	}

	static inline int32 Min(int32 A, int32 B)
	{
		return (A < B)? A : B;
	}

	static inline int32 Max(int32 A, int32 B)
	{
		return (A > B) ? A : B;
	}

	static inline float Min(float A, float B)
	{
		return (A < B) ? A : B;
	}

	static inline float Max(float A, float B)
	{
		return (A > B) ? A : B;
	}
}