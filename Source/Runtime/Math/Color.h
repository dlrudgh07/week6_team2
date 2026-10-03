#pragma once

#include "Serialization/Archive.h"

struct FColor;

// 32-bit float RGBA color
struct FLinearColor
{
    union { struct { float R, G, B, A; }; float RGBA[4]; };

    // Constructor
    FLinearColor() : R(0), G(0), B(0), A(0) {}
    FLinearColor(float InR, float InG, float InB, float InA = 1.0f) : R(InR), G(InG), B(InB), A(InA) {}
    FLinearColor(const FColor& Color);
    FLinearColor(const FVector& Vector);
    FLinearColor(const FVector4& Vector);

    // Serializer
    // TODO : FArchive 구현 필요
    // FArchive& operator<<(FArchive& Ar, FLinearColor& Color)
    // {
    //     return Ar << Color.R << Color.G << Color.B << Color.A;
    // }

    // bool Serialize(FArchive& Ar)
    // {
    //     Ar << *this;
    //     return true;
    // }

    // Operators
    inline FLinearColor operator+(const FLinearColor& ColorB) const
    {
        return FLinearColor(
            this-> R + ColorB.R,
            this-> G + ColorB.G,
            this-> B + ColorB.B,
            this-> A + ColorB.A
        );
    }

    inline FLinearColor& operator+=(const FLinearColor& ColorB)
    {
        R += ColorB.R;
        G += ColorB.G;
        B += ColorB.B;
        A += ColorB.A;
        return *this;
    }

    inline FLinearColor operator-(const FLinearColor& ColorB) const
    {
        return FLinearColor(
            this-> R - ColorB.R,
            this-> G - ColorB.G,
            this-> B - ColorB.B,
            this-> A - ColorB.A
        );
    }

    inline FLinearColor& operator-=(const FLinearColor& ColorB)
    {
        R -= ColorB.R;
        G -= ColorB.G;
        B -= ColorB.B;
        A -= ColorB.A;
        return *this;
    }

    inline FLinearColor operator*(const FLinearColor& ColorB)
    {
        return FLinearColor(
            this-> R * ColorB.R,
            this-> G * ColorB.G,
            this-> B * ColorB.B,
            this-> A * ColorB.A
        );
    }

    inline FLinearColor& operator*=(const FLinearColor& ColorB)
    {
        R *= ColorB.R;
        G *= ColorB.G;
        B *= ColorB.B;
        A *= ColorB.A;
        return *this;
    }

    inline FLinearColor operator*(const float Scalar) const
    {
        return FLinearColor(
            this-> R * Scalar,
            this-> G * Scalar,
            this-> B * Scalar,
            this-> A * Scalar
        );
    }

    inline FLinearColor& operator*(const float Scalar)
    {
        R *= Scalar;
        G *= Scalar;
        B *= Scalar;
        A *= Scalar;
        return *this;
    }

    inline FLinearColor operator/(const FLinearColor& ColorB)
    {
        return FLinearColor(
            this-> R / ColorB.R,
            this-> G / ColorB.G,
            this-> B / ColorB.B,
            this-> A / ColorB.A
        );
    }

    inline FLinearColor& operator/=(const FLinearColor& ColorB)
    {
        R /= ColorB.R;
        G /= ColorB.G;
        B /= ColorB.B;
        A /= ColorB.A;
        return *this;
    }

    inline FLinearColor operator/(const float Scalar) const
    {
        return FLinearColor(
            this-> R / Scalar,
            this-> G / Scalar,
            this-> B / Scalar,
            this-> A / Scalar
        );
    }

    inline FLinearColor& operator/(const float Scalar)
    {
        R /= Scalar;
        G /= Scalar;
        B /= Scalar;
        A /= Scalar;
        return *this;
    }
    
    inline FLinearColor GetClamped(float InMin = 0.0f, float InMax = 1.0f)
    {
        FLinearColor Ret;

        Ret.R = FMath::Clamp(R, InMin, InMax);
        Ret.G = FMath::Clamp(G, InMin, InMax);
        Ret.B = FMath::Clamp(B, InMin, InMax);
        Ret.A = FMath::Clamp(A, InMin, InMax);

        return Ret;
    }

    // Comparison operators
    bool operator==(const FLinearColor& ColorB) const
    {
        return this-> R == ColorB.R && this-> G == ColorB.G && this-> B == ColorB.B && this->A == ColorB.A; 
    }

    bool operator!=(const FLinearColor& Other) const
    {
        return this-> R != Other.R || this-> G != Other.G || this-> B != Other.B || this-> A != Other.A; 
    }

    bool Equals(const FLinearColor& ColorB, float Tolerance = 1e-4f) const
    {
        return  FMath::Abs(this->R - ColorB.R) < Tolerance &&
                FMath::Abs(this->G - ColorB.G) < Tolerance && 
                FMath::Abs(this->B - ColorB.B) < Tolerance &&
                FMath::Abs(this->A - ColorB.A) < Tolerance;
    }

    // Common Colors
    static const FLinearColor White;
    static const FLinearColor Gray;
    static const FLinearColor Black;
    static const FLinearColor Transparent;
    static const FLinearColor Red;
    static const FLinearColor Green;
    static const FLinearColor Blue;
    static const FLinearColor Yellow;
};

struct FColor
{
    union { struct { uint8 A,R,G,B; }; uint32 Bits; };

    // Constructs
    FColor() : R(0), G(0), B(0), A(0) {};
    FColor( uint8 InR, uint8 InG, uint8 InB, uint8 InA = 255): R(InR), G(InG), B(InB), A(InA) {};
    FColor( uint32 InColor ) { DWColor() = InColor; }
    // 32 Bits에 한 번에 접근하기 위한 함수
    uint32& DWColor(void) { return Bits; }
    const uint32& DWColor(void) const { return Bits; }

    // Serialzer
    // FArchive& operator<< (FArchive &Ar, FColor &Color)
    // {
    //     return Ar << Color.DWColor();
    // }

    // bool Serialize( FArchive& Ar )
    // {
    //     Ar << *this;
    //     return true;
    // }

    // Operators
    bool operator==( const FColor& C ) const
    {
        return DWColor() == C.DWColor();
    }

    bool operator!=( const FColor& C ) const
    {
        return DWColor() != C.DWColor();
    }

    inline void operator+=(const FColor& C)
    {
        R = (uint8) FMath::Min((int32) R + (int32) C.R, 255);
        G = (uint8) FMath::Min((int32) G + (int32) C.G, 255);
        B = (uint8) FMath::Min((int32) B + (int32) C.B, 255);
        A = (uint8) FMath::Min((int32) A + (int32) C.A, 255);
    }

    // Common Colors
    static const FColor White;
    static const FColor Black;
    static const FColor Transparent;
    static const FColor Red;
    static const FColor Green;
    static const FColor Blue;
    static const FColor Yellow;
    static const FColor Cyan;
    static const FColor Magenta;
    static const FColor Orange;
    static const FColor Purple;
    static const FColor Turquoise;
    static const FColor Silver;
    static const FColor Emerald;

};