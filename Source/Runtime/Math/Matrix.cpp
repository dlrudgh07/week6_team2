#include "EnginePCH.h"
#include "Matrix.h"
#include "EngineMath.h"

/* Constructor */

FMatrix::FMatrix()
{
	M[0][0] = 0; M[0][1] = 0; M[0][2] = 0; M[0][3] = 0;
	M[1][0] = 0; M[1][1] = 0; M[1][2] = 0; M[1][3] = 0;
	M[2][0] = 0; M[2][1] = 0; M[2][2] = 0; M[2][3] = 0;
	M[3][0] = 0; M[3][1] = 0; M[3][2] = 0; M[3][3] = 0;
}

FMatrix::FMatrix(const FVector4& InX, const FVector4& InY, const FVector4& InZ, const FVector4& InW)
{
	M[0][0] = InX.X; M[0][1] = InX.Y; M[0][2] = InX.Z; M[0][3] = InX.W;
	M[1][0] = InY.X; M[1][1] = InY.Y; M[1][2] = InY.Z; M[1][3] = InY.W;
	M[2][0] = InZ.X; M[2][1] = InZ.Y; M[2][2] = InZ.Z; M[2][3] = InZ.W;
	M[3][0] = InW.X; M[3][1] = InW.Y; M[3][2] = InW.Z; M[3][3] = InW.W;
}

FMatrix::FMatrix(
	const float& f00, const float& f01, const float& f02, const float& f03,
	const float& f10, const float& f11, const float& f12, const float& f13,
	const float& f20, const float& f21, const float& f22, const float& f23,
	const float& f30, const float& f31, const float& f32, const float& f33
)
{
	M[0][0] = f00; M[0][1] = f01; M[0][2] = f02; M[0][3] = f03;
	M[1][0] = f10; M[1][1] = f11; M[1][2] = f12; M[1][3] = f13;
	M[2][0] = f20; M[2][1] = f21; M[2][2] = f22; M[2][3] = f23;
	M[3][0] = f30; M[3][1] = f31; M[3][2] = f32; M[3][3] = f33;
}

/* Functions */

FMatrix FMatrix::ApplyScale(float Scale) const
{
	return FMatrix(
		FVector4(M[0][0] * Scale, M[0][1], M[0][2], M[0][3]),
		FVector4(M[1][0], M[1][1] * Scale, M[1][2], M[1][3]),
		FVector4(M[2][0], M[2][1], M[2][2] * Scale, M[2][3]),
		FVector4(M[3][0], M[3][1], M[3][2], M[3][3] * Scale)
	);
}

FVector4 FMatrix::GetOrigin()
{
	return FVector4(M[3][0], M[3][1], M[3][2], M[3][3]);
}

FMatrix FMatrix::GetTransposed() const
{
	return FMatrixRegister::Load(*this).Transpose().ToFMatrix();
}

float FMatrix::Determinant() const
{
	return FMatrixRegister::Load(*this).Determinant();
}

FMatrix FMatrix::Inverse() const
{
	return FMatrixRegister::Load(*this).Inverse().ToFMatrix();
}

FVector4 FMatrix::InverseTransformPosition(const FVector& V) const
{
	FMatrix InvMatrix = Inverse();
	return InvMatrix.TransformPosition(V);
}

FVector4 FMatrix::GetColumn(int32 i) const
{
	return FVector4(M[0][i], M[1][i], M[2][i], M[3][i]);
}

void FMatrix::SetColumn(int32 i, FVector4 Value)
{
	M[0][i] = Value[0];
	M[1][i] = Value[1];
	M[2][i] = Value[2];
	M[3][i] = Value[3];
}

void FMatrix::SetIdentity()
{
	M[0][0] = 1; M[0][1] = 0; M[0][2] = 0; M[0][3] = 0;
	M[1][0] = 0; M[1][1] = 1; M[1][2] = 0; M[1][3] = 0;
	M[2][0] = 0; M[2][1] = 0; M[2][2] = 1; M[2][3] = 0;
	M[3][0] = 0; M[3][1] = 0; M[3][2] = 0; M[3][3] = 1;
}

void FMatrix::SetOrigin(const FVector& NewOrigin)
{
	M[3][0] = NewOrigin.X;
	M[3][1] = NewOrigin.Y;
	M[3][2] = NewOrigin.Z;
}

FMatrix FMatrix::MakeTranslation(const FVector& T)
{
	FMatrix Mat;
	Mat.SetIdentity();
	Mat.M[3][0] = T.X;
	Mat.M[3][1] = T.Y;
	Mat.M[3][2] = T.Z;
	return Mat;
}

FVector4 FMatrix::TransformFVector4(const FVector4& V) const
{
	const FVectorRegister Result = MultiplyRow(VectorSIMD::Load(V.V), FMatrixRegister::Load(*this));

	FVector4 Out;
	VectorSIMD::Store(Out.V, Result);
	return Out;
}

FVector FMatrix::TransformPosition(const FVector& V) const
{
	FVector4 Result = TransformFVector4(FVector4(V.X, V.Y, V.Z, 1.0f));
	return FVector(Result.X, Result.Y, Result.Z);
}

FVector FMatrix::TransformVector(const FVector& V) const
{
	FVector4 Result = TransformFVector4(FVector4(V.X, V.Y, V.Z, 0.0f));
	return FVector(Result.X, Result.Y, Result.Z);
}

void FMatrix::SetAxes(const FVector4& Axis0, const FVector4& Axis1, const FVector4& Axis2, const FVector4& Axis3)
{
	// 수정 필요
	M[0][0] = Axis0.X; M[0][1] = Axis0.Y; M[0][2] = Axis0.Z; M[0][3] = Axis0.W;
	M[1][0] = Axis1.X; M[1][1] = Axis1.Y; M[1][2] = Axis1.Z; M[1][3] = Axis1.W;
	M[2][0] = Axis2.X; M[2][1] = Axis2.Y; M[2][2] = Axis2.Z; M[2][3] = Axis2.W;
	M[3][0] = Axis3.X; M[3][1] = Axis3.Y; M[3][2] = Axis3.Z; M[3][3] = Axis3.W;
}

void FMatrix::SetAxis(int32 i, const FVector& Axis)
{
	M[i][0] = Axis.X;
	M[i][1] = Axis.Y;
	M[i][2] = Axis.Z;
}

void FMatrix::GetUnitAxis(FVector4& X, FVector4& Y, FVector4& Z) const
{
	X = X.Normalized();
	Y = Y.Normalized();
	Z = Z.Normalized();
}

FMatrix FMatrix::Multiply(const FMatrix& Other) const
{
	FMatrixRegister A = FMatrixRegister::Load(*this);
	FMatrixRegister B = FMatrixRegister::Load(Other);

	FMatrixRegister Result;

	for (int32 i = 0; i < 4; ++i)
	{
		FVectorRegister X = VectorSIMD::Mul(VectorSIMD::Swizzle<0, 0, 0, 0>(A.R[i]), B.R[0]);
		FVectorRegister Y = VectorSIMD::Mul(VectorSIMD::Swizzle<1, 1, 1, 1>(A.R[i]), B.R[1]);
		FVectorRegister Z = VectorSIMD::Mul(VectorSIMD::Swizzle<2, 2, 2, 2>(A.R[i]), B.R[2]);
		FVectorRegister W = VectorSIMD::Mul(VectorSIMD::Swizzle<3, 3, 3, 3>(A.R[i]), B.R[3]);

		Result.R[i] = VectorSIMD::Add(VectorSIMD::Add(X, Y), VectorSIMD::Add(Z, W));
	}
	 
	return Result.ToFMatrix();
}

/* Operator */

// operator= 는 Matrix.h에서 = default 로 선언 (원소 복사, 헤더에서 인라인됨)

FMatrix FMatrix::operator - ()
{
	FMatrixRegister M = FMatrixRegister::Load(*this);

	M.R[0] = VectorSIMD::Negate(M.R[0]);
	M.R[1] = VectorSIMD::Negate(M.R[1]);
	M.R[2] = VectorSIMD::Negate(M.R[2]);
	M.R[3] = VectorSIMD::Negate(M.R[3]);

	return M.ToFMatrix();
}

const float* FMatrix::operator[] (int32 Index) const
{
	return M[Index];
}

float* FMatrix::operator[] (int32 Index)
{
	return M[Index];
}

bool FMatrix::operator == (const FMatrix& Other) const
{
	FMatrixRegister A = FMatrixRegister::Load(*this);
	FMatrixRegister B = FMatrixRegister::Load(Other);

	return
		VectorSIMD::IsNearlyEqual(A.R[0], B.R[0]) &&
		VectorSIMD::IsNearlyEqual(A.R[1], B.R[1]) &&
		VectorSIMD::IsNearlyEqual(A.R[2], B.R[2]) &&
		VectorSIMD::IsNearlyEqual(A.R[3], B.R[3]);
}

bool FMatrix::operator != (const FMatrix& Other) const
{
	return !(*this == Other);
}

FMatrix FMatrix::operator * (const FMatrix& Other) const
{
	return Multiply(Other);
}

FMatrix FMatrix::operator * (const float& Other) const
{
	FMatrixRegister M = FMatrixRegister::Load(*this);

	FVectorRegister Scalar = VectorSIMD::SetVal(Other);

	M.R[0] = VectorSIMD::Mul(M.R[0], Scalar);
	M.R[1] = VectorSIMD::Mul(M.R[1], Scalar);
	M.R[2] = VectorSIMD::Mul(M.R[2], Scalar);
	M.R[3] = VectorSIMD::Mul(M.R[3], Scalar);

	return M.ToFMatrix();

}

FVector4 FMatrix::operator * (const FVector4& Other) const
{
	return TransformFVector4(Other);
}

FMatrix& FMatrix::operator *= (const FMatrix& Other)
{
	*this = *this * Other;
	return *this;
}

FMatrix& FMatrix::operator *= (float Other)
{
	*this = *this * Other;
	return *this;
}

FMatrix FMatrix::operator + (const FMatrix& Other) const
{
	FMatrixRegister A = FMatrixRegister::Load(*this);
	FMatrixRegister B = FMatrixRegister::Load(Other);

	FMatrixRegister Result;

	Result.R[0] = VectorSIMD::Add(A.R[0], B.R[0]);
	Result.R[1] = VectorSIMD::Add(A.R[1], B.R[1]);
	Result.R[2] = VectorSIMD::Add(A.R[2], B.R[2]);
	Result.R[3] = VectorSIMD::Add(A.R[3], B.R[3]);

	return Result.ToFMatrix();
}

FMatrix& FMatrix::operator += (const FMatrix& Other)
{
	*this = *this + Other;
	return *this;
}

FMatrix FMatrix::operator - (const FMatrix& Other) const
{
	FMatrixRegister A = FMatrixRegister::Load(*this);
	FMatrixRegister B = FMatrixRegister::Load(Other);

	FMatrixRegister Result;

	Result.R[0] = VectorSIMD::Sub(A.R[0], B.R[0]);
	Result.R[1] = VectorSIMD::Sub(A.R[1], B.R[1]);
	Result.R[2] = VectorSIMD::Sub(A.R[2], B.R[2]);
	Result.R[3] = VectorSIMD::Sub(A.R[3], B.R[3]);

	return Result.ToFMatrix();
}

FMatrix& FMatrix::operator -= (const FMatrix& Other)
{
	*this = *this - Other;
	return *this;
}


FMatrix FMatrix::Identity = FMatrix(
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
);

/* Global Operator */
std::ostream& operator << (std::ostream& OS, const FMatrix& M)
{
	OS << M[0][0] << " " << M[0][1] << " " << M[0][2] << " " << M[0][3] << "\n";
	OS << M[1][0] << " " << M[1][1] << " " << M[1][2] << " " << M[1][3] << "\n";
	OS << M[2][0] << " " << M[2][1] << " " << M[2][2] << " " << M[2][3] << "\n";
	OS << M[3][0] << " " << M[3][1] << " " << M[3][2] << " " << M[3][3] << "\n";
	return OS;
}

FRotator MatrixToRotator(const FMatrix& Mat)
{
	FRotator R;
	R.Roll = atan2f(Mat.M[1][2], Mat.M[2][2]) * 180.0f / PI;
	R.Pitch = atan2f(-Mat.M[0][2], sqrtf(Mat.M[1][2] * Mat.M[1][2] + Mat.M[2][2] * Mat.M[2][2])) * 180.0f / PI;
	R.Yaw = atan2f(Mat.M[0][1], Mat.M[0][0]) * 180.0f / PI;
	return R;
}