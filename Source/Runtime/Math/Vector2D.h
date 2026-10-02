#pragma once

#include "HAL/Platform.h"

struct FVector2D
{
	union
	{
		float V[2];
		struct
		{
			float X;
			float Y;

		};
	};

	FVector2D()
		:X(0), Y(0){}
	FVector2D(float InX, float InY)
		:X(InX), Y(InY) {
	}
	explicit FVector2D(float InValue)
		:FVector2D(InValue, InValue) {
	}

	float& operator[](uint64 _index) { return V[_index]; }
	const float& operator[](uint64 _index) const { return V[_index]; }

	FVector2D operator-(const FVector2D& Other) { return FVector2D(X - Other.X, Y - Other.Y); }
	FVector2D operator-(const FVector2D& Other) const { return FVector2D(X - Other.X, Y - Other.Y); }

	FVector2D operator+(const FVector2D& Other) { return FVector2D(X + Other.X, Y + Other.Y); }
	FVector2D operator+(const FVector2D& Other) const { return FVector2D(X + Other.X, Y + Other.Y); }

	FVector2D operator*(const float Scalar) { return FVector2D(X * Scalar, Y * Scalar); }
	FVector2D operator*(const float Scalar) const { return FVector2D(X * Scalar, Y * Scalar); }
};