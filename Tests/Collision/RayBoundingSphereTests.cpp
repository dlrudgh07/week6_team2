#include "EnginePCH.h"
#include "Collision/Ray.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>

namespace
{
    int Checks = 0;

    void Check(const bool Condition, const char* Name)
    {
        ++Checks;
        if (!Condition)
        {
            std::fprintf(stderr, "FAILED: %s (check %d)\n", Name, Checks);
            std::exit(1);
        }
    }

    void CheckCase(const char* Name, const FRay& Ray, const FVector& Center,
        const float Radius, const bool ExpectedHit, const double ExpectedT = 0.0)
    {
        constexpr float Sentinel = -123.0f;
        float T = Sentinel;
        const bool Hit = RayIntersectsBoundingSphere(Ray, Center, Radius, T);
        Check(Hit == ExpectedHit, Name);
        Check(Hit ? std::abs(static_cast<double>(T) - ExpectedT) <= 2e-4 * std::max(1.0, std::abs(ExpectedT))
                  : T == Sentinel, Name);

        FTraceContext Context = MakeTraceContext(Ray, nullptr, nullptr);
        Context.BestDistance = 0.0f; // AABB와 동일하게 이 함수는 거리 제한을 적용하지 않는다.
        float ContextT = Sentinel;
        Check(RayIntersectsBoundingSphere(Context, Center, Radius, ContextT) == Hit, Name);
        Check(ContextT == T, Name);
    }

    // 독립적인 double 기준 구현: 중심을 Ray에 투영한 뒤 구의 반현 길이를 구한다.
    bool Reference(const FRay& Ray, const FVector& Center, const float Radius, double& T)
    {
        if (Radius < 0.0f) return false;
        double O[3], D[3], LengthSquared = 0.0, DistanceSquared = 0.0, Projection = 0.0;
        for (int i = 0; i < 3; ++i)
        {
            O[i] = static_cast<double>(Ray.Origin.V[i]) - Center.V[i];
            D[i] = Ray.Direction.V[i];
            DistanceSquared += O[i] * O[i];
            LengthSquared += D[i] * D[i];
            Projection -= O[i] * D[i];
        }
        const double R2 = static_cast<double>(Radius) * Radius;
        if (DistanceSquared <= R2) { T = 0.0; return true; }
        if (LengthSquared == 0.0 || Projection <= 0.0) return false;
        Projection /= LengthSquared;
        double ClosestSquared = 0.0;
        for (int i = 0; i < 3; ++i)
        {
            const double Coordinate = O[i] + Projection * D[i];
            ClosestSquared += Coordinate * Coordinate;
        }
        if (ClosestSquared > R2) return false;
        T = Projection - std::sqrt((R2 - ClosestSquared) / LengthSquared);
        return true;
    }
}

int main()
{
    const FVector Center(0.0f, 0.0f, 0.0f);
    CheckCase("front", {{-3, 0, 0}, {1, 0, 0}}, Center, 1, true, 2);
    CheckCase("non-unit", {{-3, 0, 0}, {2, 0, 0}}, Center, 1, true, 1);
    CheckCase("small direction", {{-3, 0, 0}, {0.001f, 0, 0}}, Center, 1, true, 2000);
    CheckCase("inside", {{0.5f, 0, 0}, {1, 0, 0}}, Center, 1, true);
    CheckCase("surface outward", {{1, 0, 0}, {1, 0, 0}}, Center, 1, true);
    CheckCase("surface inward", {{1, 0, 0}, {-1, 0, 0}}, Center, 1, true);
    CheckCase("tangent", {{-3, 1, 0}, {1, 0, 0}}, Center, 1, true, 3);
    CheckCase("parallel miss", {{-3, 2, 0}, {1, 0, 0}}, Center, 1, false);
    CheckCase("behind", {{3, 0, 0}, {1, 0, 0}}, Center, 1, false);
    CheckCase("zero direction outside", {{3, 0, 0}, {0, 0, 0}}, Center, 1, false);
    CheckCase("zero direction inside", {{0, 0, 0}, {0, 0, 0}}, Center, 1, true);
    CheckCase("negative radius", {{0, 0, 0}, {1, 0, 0}}, Center, -1, false);
    CheckCase("point sphere", {{-3, 0, 0}, {1, 0, 0}}, Center, 0, true, 3);
    CheckCase("translated", {{7, 4, 5}, {1, 0, 0}}, {10, 4, 5}, 1, true, 2);
    CheckCase("near surface", {{-1.0001f, 0, 0}, {1, 0, 0}}, Center, 1, true, 0.0001000166);
    CheckCase("far small hit", {{-100000, 0, 0}, {1, 0, 0}}, Center, 0.5f, true, 99999.5);
    CheckCase("far small miss", {{-100000, 1, 0}, {1, 0, 0}}, Center, 0.5f, false);

    std::mt19937 Random(20260930);
    std::uniform_real_distribution<float> Position(-50.0f, 50.0f);
    std::uniform_real_distribution<float> Direction(-4.0f, 4.0f);
    std::uniform_real_distribution<float> Radius(0.01f, 5.0f);
    for (int i = 0; i < 100000; ++i)
    {
        const FVector C(Position(Random), Position(Random), Position(Random));
        FRay Ray{{Position(Random), Position(Random), Position(Random)},
                 {Direction(Random), Direction(Random), Direction(Random)}};
        if (i % 2 == 0)
        {
            Ray.Direction.X = C.X - Ray.Origin.X + Direction(Random);
            Ray.Direction.Y = C.Y - Ray.Origin.Y + Direction(Random);
            Ray.Direction.Z = C.Z - Ray.Origin.Z + Direction(Random);
        }
        const float R = Radius(Random);
        double T = 0.0;
        const bool Hit = Reference(Ray, C, R, T);
        CheckCase("random double reference", Ray, C, R, Hit, T);
    }
    std::printf("PASS: 17 boundary cases + 100000 reference cases; %d checks, both overloads.\n", Checks);
}
