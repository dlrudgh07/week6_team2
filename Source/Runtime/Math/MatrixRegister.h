#pragma once

#include "Matrix.h"
#include "VectorRegister.h"

struct FMatrixRegister 
{
	FVectorRegister R[4];
public:
	FMatrixRegister();
	FMatrixRegister(FVectorRegister R0, FVectorRegister R1, FVectorRegister R2, FVectorRegister R3) : R{ R0,R1,R2,R3 } {};

	FMatrix ToFMatrix() const;
	FMatrixRegister Transpose() const;
	
	float Determinant3x3(FVectorRegister A, FVectorRegister B, FVectorRegister C) const;

	// 행렬식
	float Determinant() const;
	// 역행렬
	FMatrixRegister Inverse() const;

	

	//헤더구현이유 : 컴파일단계에서 인라인되도록(파일경계를 넘는 호출제거)
	static FMatrixRegister Load(const FMatrix& M)
	{
		return FMatrixRegister(VectorSIMD::Load(M.M[0]), VectorSIMD::Load(M.M[1]),
			VectorSIMD::Load(M.M[2]), VectorSIMD::Load(M.M[3]));
	}	// 레지스터 4개 -> 지정한 FMatrix에 바로 저장 (임시 FMatrix를 만들지 않음)
	void Store(FMatrix& Out) const
	{
		VectorSIMD::Store(Out.M[0], R[0]);
		VectorSIMD::Store(Out.M[1], R[1]);
		VectorSIMD::Store(Out.M[2], R[2]);
		VectorSIMD::Store(Out.M[3], R[3]);
	}
	static FMatrixRegister Identity()
	{
		return FMatrixRegister(VectorSIMD::SetVal(1, 0, 0, 0), VectorSIMD::SetVal(0, 1, 0, 0),
			VectorSIMD::SetVal(0, 0, 1, 0), VectorSIMD::SetVal(0, 0, 0, 1));

	}
	
};

inline FVectorRegister MultiplyRow(FVectorRegister Row, const FMatrixRegister& B)
{
	using namespace VectorSIMD;
	return Add(Add(Mul(SplatX(Row), B.R[0]), Mul(SplatY(Row), B.R[1])),
		Add(Mul(SplatZ(Row), B.R[2]), Mul(SplatW(Row), B.R[3])));
}
inline FMatrixRegister operator*(const FMatrixRegister& A, const FMatrixRegister& B)
{
	return FMatrixRegister(MultiplyRow(A.R[0], B), MultiplyRow(A.R[1], B),
		MultiplyRow(A.R[2], B), MultiplyRow(A.R[3], B));
}