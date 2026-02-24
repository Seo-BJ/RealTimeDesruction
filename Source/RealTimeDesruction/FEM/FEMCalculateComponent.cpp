// Fill out your copyright notice in the Description page of Project Settings.


#include "FEMCalculateComponent.h"

#include "FTetWildWrapper.h"
#include "Engine/StaticMesh.h"
#include "HAL/PlatformTLS.h"
#include <atomic>

#include "FTetWildWrapper.h"

using namespace Eigen;

UFEMCalculateComponent::UFEMCalculateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFEMCalculateComponent::BeginPlay()
{
	Super::BeginPlay();
    InitializeTetMesh();
}

void UFEMCalculateComponent::InitializeTetMesh()
{
    // StaticMesh 데이터 준비
    TArray<FVector> Verts;
    TArray<FIntVector3> Tris;

    // Todo : Get Verts and Tris
    if (UStaticMeshComponent* StaticMeshComponent = GetOwner()->FindComponentByClass<UStaticMeshComponent>())
    {
        if (UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh())
        {
            // Access LOD 0 for simplicity, modify if you need other LODs
            const FStaticMeshLODResources& LODResource = StaticMesh->GetRenderData()->LODResources[0];

            // Get vertices
            const FPositionVertexBuffer& VertexBuffer = LODResource.VertexBuffers.PositionVertexBuffer;
            const int32 VertexCount = VertexBuffer.GetNumVertices();
            Verts.Reserve(VertexCount);
            for (int32 i = 0; i < VertexCount; i++)
            {
                double PositionX = VertexBuffer.VertexPosition(i).X;
                double PositionY = VertexBuffer.VertexPosition(i).Y;
                double PositionZ = VertexBuffer.VertexPosition(i).Z;
                FVector PositionVector = FVector(PositionX, PositionY, PositionZ);
                Verts.Add(PositionVector);
            }

            // Get triangle indices
            const FIndexArrayView& IndexArray = LODResource.IndexBuffer.GetArrayView();
            const int32 IndexCount = IndexArray.Num();
            Tris.Reserve(IndexCount / 3);
            for (int32 i = 0; i < IndexCount; i += 3)
            {
                Tris.Add(FIntVector3(IndexArray[i], IndexArray[i + 1], IndexArray[i + 2]));
            }
        }
    }
    
    UE::Geometry::FTetWild::FTetMeshParameters Params;
    Params.bCoarsen = bCoarsen;
    Params.bExtractManifoldBoundarySurface = bExtractManifoldBoundarySurface;
    Params.bSkipSimplification = bSkipSimplification;

    Params.EpsRel = EpsRel;
    Params.MaxIts = MaxIterations;
    Params.StopEnergy = StopEnergy;
    Params.IdealEdgeLength = IdealEdgeLength;

    Params.bInvertOutputTets = bInvertOutputTets;

    FProgressCancel Progress;
    UE_LOG(LogTemp, Warning, TEXT("Generating tet mesh via TetWild..."));
    if (UE::Geometry::FTetWild::ComputeTetMesh(Params, Verts, Tris, TetMeshVertices, Tets, &Progress))
    {
        UE_LOG(LogTemp, Warning, TEXT("Succefully Generated tet mesh!"));

        UE_LOG(LogTemp, Display, TEXT("Vertex : %d"), TetMeshVertices.Num());
        UE_LOG(LogTemp, Display, TEXT("Tets : %d"), Tets.Num());

        AActor* OnwerActor = GetOwner();
        UFEMCalculateComponent* FEMComponent = OnwerActor->FindComponentByClass<UFEMCalculateComponent>();
        SetUndeformedPositions();
        KMatrix();

        GenerateGraphFromTets();

        // 성능 벤치마크 실행 (초기화 완료 후)
        if (bEnableProfiling)
        {
            BenchmarkSearchPerformance();
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to Generate tet mesh!"));
    }
}


void UFEMCalculateComponent::GenerateGraphFromTets()
{
    // 모든 정점을 그래프에 추가
    for (uint32 i = 0; i < (uint32)TetMeshVertices.Num(); i++)
        Graph.addVertex(i);

    // 각 사면체의 6개 에지를 그래프에 추가
    // 사면체는 4개 정점으로 이루어지며 총 6개의 에지를 가짐
    for (const FIntVector4 Tet : Tets)
    {
        FVector Vertex_X = TetMeshVertices[Tet.X];
        FVector Vertex_Y = TetMeshVertices[Tet.Y];
        FVector Vertex_Z = TetMeshVertices[Tet.Z];
        FVector Vertex_W = TetMeshVertices[Tet.W];

        // 6개 에지 추가: (X-Y, Y-Z, Z-X, X-W, Y-W, Z-W)
        Graph.addLink(Tet.X, Tet.Y, Vertex_Y - Vertex_X);
        Graph.addLink(Tet.Y, Tet.Z, Vertex_Z - Vertex_Y);
        Graph.addLink(Tet.Z, Tet.X, Vertex_X - Vertex_Z);
        Graph.addLink(Tet.X, Tet.W, Vertex_W - Vertex_X);
        Graph.addLink(Tet.Y, Tet.W, Vertex_W - Vertex_Y);
        Graph.addLink(Tet.Z, Tet.W, Vertex_W - Vertex_Z);
    }
}

float UFEMCalculateComponent::CalculateEnergyAtTatUsingFEM(const FVector& Velocity, const FVector& NextTickVelocity, const float Mass, const FVector& HitPoint)
{
    // 1. 충돌 지점에서 가장 가까운 사면체와 삼각형 면 찾기
    int32 ExcludedIndex = 0;
    FInt32Vector4 ClosestResult = GetClosestTriangleAndTet(HitPoint, ExcludedIndex);
    int32 TargetTetIndex = ClosestResult[0];

    // 충돌이 발생한 삼각형의 세 정점 인덱스 저장
    CurrentImpactPoint = {
        (uint32)Tets[TargetTetIndex][ClosestResult[1] - 1],
        (uint32)Tets[TargetTetIndex][ClosestResult[2] - 1],
        (uint32)Tets[TargetTetIndex][ClosestResult[3] - 1]
    };

    // 2. 충돌 삼각형의 세 정점 위치
    FVector A = TetMeshVertices[CurrentImpactPoint[0]];
    FVector B = TetMeshVertices[CurrentImpactPoint[1]];
    FVector C = TetMeshVertices[CurrentImpactPoint[2]];

    // 3. 충격력 벡터 F 계산 (9x1: 세 정점의 x,y,z 힘)
    Matrix<float, 9, 1> F = CalculateImpactForceMatrix(Velocity, NextTickVelocity, Mass, HitPoint, { A, B, C });

    // 4. 고정점을 제외한 9x9 강성 행렬 추출
    Matrix<float, 9, 9> K = SubKMatrix(KElements[TargetTetIndex], ExcludedIndex);

    // 5. Ku = F 선형 시스템을 풀어 변위 u 계산
    Matrix<float, 4, 4> u = UMatrix(K, F, { ClosestResult[1], ClosestResult[2], ClosestResult[3] }, ExcludedIndex);

    // 6. 변형 전 위치 행렬 Dm 구성
    Matrix<float, 4, 4> Dm;     // 4x4 동차 좌표 행렬
    Matrix<float, 3, 4> Dm2;    // 3x4 위치 행렬 (Jacobian 계산용)
    TArray<int32> VertexIndex;
    VertexIndex.Add(Tets[TargetTetIndex].X);
    VertexIndex.Add(Tets[TargetTetIndex].Y);
    VertexIndex.Add(Tets[TargetTetIndex].Z);
    VertexIndex.Add(Tets[TargetTetIndex].W);

    // 변형 전 위치를 행렬로 구성
    for (int vtx = 0; vtx < 4; vtx++)
    {
        for (int dim = 0; dim < 3; dim++)
        {
            Dm(dim, vtx) = UndeformedPositions[3 * VertexIndex[vtx] + dim]/ 100;
            Dm2(dim, vtx) = UndeformedPositions[3 * VertexIndex[vtx] + dim]/ 100;
        }
    }
    // 동차 좌표를 위한 네 번째 행
    for (int j = 0; j < 4; j++)
    {
        Dm(3, j) = 1;
    }

    // 7. Jacobian 행렬로부터 사면체 부피 계산
    const Matrix<float, 3, 3> Jaco = Jacobian(Dm2);
    float TargetTetVolume = GetTetVolume(Jaco);

    // 8. 최종 에너지 계산 (변형 구배 → 변형률 텐서 → 에너지 밀도 → 총 에너지)
    return CalculateEnergy(Dm, u, TargetTetVolume);
}
void UFEMCalculateComponent::BenchmarkSearchPerformance()
{
    if (Tets.Num() == 0 || TetMeshVertices.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Benchmark] No tet mesh available for benchmarking"));
        return;
    }

    // 메쉬의 바운딩 박스 중심에서 약간 떨어진 테스트 지점 생성
    FVector MeshCenter = FVector::ZeroVector;
    for (const FVector& Vertex : TetMeshVertices)
    {
        MeshCenter += Vertex;
    }
    MeshCenter /= TetMeshVertices.Num();

    // 테스트 포인트: 메쉬 중심에서 약간 오프셋
    FVector TestPoint = MeshCenter + FVector(50.0f, 0.0f, 0.0f);

    UE_LOG(LogTemp, Warning, TEXT("========================================"));
    UE_LOG(LogTemp, Warning, TEXT("[Benchmark] Strict Accuracy & Performance Test"));
    UE_LOG(LogTemp, Warning, TEXT("[Benchmark] Iterations per Method: 10 (All Verified)"));
    UE_LOG(LogTemp, Warning, TEXT("[Benchmark] Tet Count: %d"), Tets.Num());
    UE_LOG(LogTemp, Warning, TEXT("[Benchmark] Test Position: %s"), *TestPoint.ToString());
    UE_LOG(LogTemp, Warning, TEXT("========================================"));

    const int32 NumIterations = 10;

    // -------------------------------------------------------
    // [Helper Lambda] 결과값으로부터 실제 거리 계산 (Tie-Break 검증용)
    // -------------------------------------------------------
    auto CalculateDistanceFromResult = [&](const FInt32Vector4& Result, const FVector& HitPos) -> float
    {
        if (Result[0] < 0 || Result[0] >= Tets.Num()) return FLT_MAX;

        const FIntVector4& Tet = Tets[Result[0]];
        
        FVector A = TetMeshVertices[Tet.X];
        FVector B = TetMeshVertices[Tet.Y];
        FVector C = TetMeshVertices[Tet.Z];
        FVector D = TetMeshVertices[Tet.W];

        float D1 = DistanceToTriangle(HitPos, A, B, C);
        float D2 = DistanceToTriangle(HitPos, A, B, D);
        float D3 = DistanceToTriangle(HitPos, A, C, D);
        float D4 = DistanceToTriangle(HitPos, B, C, D);

        // FMath::Min은 initializer list를 지원하지 않으므로 중첩 사용
        return FMath::Min(D1, FMath::Min(D2, FMath::Min(D3, D4)));
    };

    // -------------------------------------------------------
    // 1. Baseline 설정 (Sequential 1회 실행)
    // 정답지(BaseResult)와 기준 시간(Time_Seq)을 먼저 구합니다.
    // -------------------------------------------------------
    int32 BaseExcludedIndex = 0;
    FInt32Vector4 BaseResult;
    double Time_Seq = 0.0;

    // 정답 데이터 생성 (Sequential 1회 실행)
    BaseResult = GetClosestTriangleAndTetSequential(TestPoint, BaseExcludedIndex);
    float BaseDistance = CalculateDistanceFromResult(BaseResult, TestPoint);

    // 기준 시간 측정 (Sequential N회 실행 평균)
    for (int32 i = 0; i < NumIterations; ++i)
    {
        int32 TempExcluded = 0;
        double Start = FPlatformTime::Seconds();
        // 최적화를 막기 위해 결과를 volatile 변수에 할당하거나 사용해야 하지만, 
        // 여기서는 함수 호출 오버헤드 자체가 측정 대상이므로 단순 호출
        GetClosestTriangleAndTetSequential(TestPoint, TempExcluded);
        Time_Seq += (FPlatformTime::Seconds() - Start) * 1000.0;
    }
    Time_Seq /= NumIterations;

    UE_LOG(LogTemp, Display, TEXT("Baseline Calculation: Index=%d, Excluded=%d, Dist=%.4f, AvgTime=%.3f ms"), 
        BaseResult[0], BaseExcludedIndex, BaseDistance, Time_Seq);

    // -------------------------------------------------------
    // 2. 각 방법별 테스트 및 전수 검증 람다 함수
    // -------------------------------------------------------
    auto RunTest = [&](const FString& MethodName, TFunction<FInt32Vector4(const FVector&, int32&)> Func)
    {
        double TotalTime = 0.0;
        bool bHasMismatch = false;
        bool bHasTie = false; // 거리는 같지만 인덱스가 다른 경우

        for (int32 i = 0; i < NumIterations; ++i)
        {
            int32 CurrentExcluded = 0;
            double Start = FPlatformTime::Seconds();
            
            // 알고리즘 실행
            FInt32Vector4 CurrentResult = Func(TestPoint, CurrentExcluded);
            
            TotalTime += (FPlatformTime::Seconds() - Start) * 1000.0;

            // [검증 1] 완전 일치 여부 확인
            if (CurrentResult[0] != BaseResult[0] || CurrentExcluded != BaseExcludedIndex)
            {
                // [검증 2] 불일치 시, 거리(Physics) 기반 Tie-Break 확인
                float CurrentDist = CalculateDistanceFromResult(CurrentResult, TestPoint);
                
                // 오차 범위(Tolerance) 내에서 거리가 같다면 정답으로 인정
                if (FMath::IsNearlyEqual(CurrentDist, BaseDistance, 0.001f))
                {
                    bHasTie = true; // 물리적으로는 정답임
                }
                else
                {
                    bHasMismatch = true;
                    UE_LOG(LogTemp, Error, TEXT("  [%s] FAILED at Iteration %d!"), *MethodName, i);
                    UE_LOG(LogTemp, Error, TEXT("    Expected: Tet=%d, Excluded=%d (Dist=%.4f)"), 
                        BaseResult[0], BaseExcludedIndex, BaseDistance);
                    UE_LOG(LogTemp, Error, TEXT("    Actual:   Tet=%d, Excluded=%d (Dist=%.4f)"), 
                        CurrentResult[0], CurrentExcluded, CurrentDist);
                }
            }
        }

        double AvgTime = TotalTime / NumIterations;
        double Speedup = Time_Seq / AvgTime;
        
        FString Status = TEXT("PASS");
        if (bHasMismatch) Status = TEXT("FAIL");
        else if (bHasTie) Status = TEXT("PASS (Tie Found)");

        UE_LOG(LogTemp, Warning, TEXT("  %-30s : %.3f ms | Speedup: %.2fx | Result: %s"), 
            *MethodName, AvgTime, Speedup, *Status);
    };

    // -------------------------------------------------------
    // 3. 실제 테스트 실행
    // -------------------------------------------------------

    // [추가됨] 1. Sequential (Self-Test)
    // 자기 자신과 비교하므로 Speedup은 1.0x 근처, Result는 무조건 PASS여야 함
    RunTest(TEXT("Sequential (Self-Check)"), [&](const FVector& P, int32& E) { 
        return GetClosestTriangleAndTetSequential(P, E); 
    });

    // 2. Parallel (Atomic)
    RunTest(TEXT("Parallel (Atomic)"), [&](const FVector& P, int32& E) { 
        return GetClosestTriangleAndTetParallel(P, E); 
    });

    // 3. Parallel_Mutex
    RunTest(TEXT("Parallel_Mutex"), [&](const FVector& P, int32& E) { 
        return GetClosestTriangleAndTetParallel_Mutex(P, E); 
    });

    // 4. Parallel_LockFree (Chunk-based)
    RunTest(TEXT("Parallel_LockFree (Chunk)"), [&](const FVector& P, int32& E) {
        return GetClosestTriangleAndTetParallel_LockFree(P, E);
    });
    
    UE_LOG(LogTemp, Warning, TEXT("========================================"));
}
float UFEMCalculateComponent::CalculateEnergy(Matrix<float, 4, 4> DmMatrix, Matrix<float, 4, 4> UMatrix, float TetVolume) const
{
    // 변형 후 위치 행렬: Ds = Dm + u
    Matrix<float, 4, 4> DsMatrix = DmMatrix + UMatrix;
    //LogMatrix<Matrix<float, 4, 4>>(DmMatrix, "Dm Matrix");
    //LogMatrix<Matrix<float, 4, 4>>(UMatrix, "U Matrix");
    //LogMatrix<Matrix<float, 4, 4>>(DsMatrix, "Ds Matrix");

    // 변형 구배(Deformation Gradient) 계산: G = Ds * Dm^-1
    Matrix<float, 4, 4> GMatrix = DsMatrix * DmMatrix.inverse();

    // 단위 행렬 생성
    Matrix<float, 4, 4> Identity;
    Identity.setZero();
    Identity(0, 0) = 1;
    Identity(1, 1) = 1;
    Identity(2, 2) = 1;
    Identity(3, 3) = 1;

    // 변형률 텐서(Strain Tensor) 계산: ε = (G + G^T)/2 - I
    // 선형 탄성 모델에서 변형률은 대칭 행렬
    Matrix<float, 4, 4> StrainTensorMatrix = (GMatrix + GMatrix.transpose()) / 2 - Identity;

    // 이중 내적 계산: ε:ε = Σ(εij * εij)
    float DoubleDotProduct = 0.f;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            DoubleDotProduct += (StrainTensorMatrix(i, j) * StrainTensorMatrix(i, j));
        }
    }

    // 대각합(Trace) 계산: tr(ε) = Σεii
    const float Trace = StrainTensorMatrix(0,0) + StrainTensorMatrix(1, 1) + StrainTensorMatrix(2, 2) + StrainTensorMatrix(3, 3);

    // 에너지 밀도 계산: ψ = μ(ε:ε) + (λ/2)(tr(ε))²
    // μ: 전단 계수 (형상 변화에 대한 저항)
    // λ: 첫 번째 Lamé 파라미터 (부피 변화에 대한 저항)
    const float EnergyDensity = Mu * DoubleDotProduct + 0.5 * Lambda * Trace * Trace;

    // 총 에너지: E = ψ * V
    const float Energy = EnergyDensity * TetVolume;

    return Energy;
}

Matrix<float, 4, 4> UFEMCalculateComponent::UMatrix(Matrix<float, 9, 9> KMatrix, Matrix<float, 9, 1> FMatrix, const FInt32Vector3 TriangleIndex, int32 ExcludedIndex)
{
    Matrix<float, 9, 1> UMatrix9x1;
    Matrix<float, 4, 4> UMatrix4x4;
    UMatrix9x1.setZero();
    UMatrix4x4.setZero();

    // Ku = F 시스템 풀이: u = K^-1 * F
    UMatrix9x1 = KMatrix.inverse() * FMatrix;
   //LogMatrix<Matrix<float, 9, 1>>(UMatrix9x1, "U Matrix 9 x 1");
   //LogMatrix<Matrix<float, 9, 9>>(KMatrix.inverse(), "K Inverse");
    UE_LOG(LogTemp, Warning, TEXT("Sub K Determinant : %f"), KMatrix.determinant());

    // 9x1 변위 벡터를 4x4 행렬로 재구성
    for (int Index = 0; Index < 4; Index++)
    {
        for (int Dim = 0; Dim < 3; Dim++)
        {
            if (Index == ExcludedIndex)
            {
                // 고정점의 변위는 0
                UMatrix4x4(Dim, Index) = 0;
            }
            else
            {
                // 나머지 정점의 변위 할당
                UMatrix4x4(Dim, Index) = UMatrix9x1(3 * Index + Dim, 0) / 100;
            }
        }
    }
    // 동차 좌표를 위한 네 번째 행
    for (int col = 0; col < 4; col++)
    {
        UMatrix4x4(3, col) = 1;
    }
    return UMatrix4x4;
}

Matrix<float, 9, 1> UFEMCalculateComponent::CalculateImpactForceMatrix(const FVector& InitialVelocity, const FVector& NextTickVelocity, const float Mass, const FVector& HitPoint, const TArray<FVector>& TraignleVertices)
{
    const float DeltaTime = 0.01;
    const FVector DeltaVelocity = NextTickVelocity - InitialVelocity;
    // 충격력 계산: F = m * Δv / Δt
    const FVector ImpactForce = (Mass * DeltaVelocity) / DeltaTime;

    Matrix<float, 9, 1> ImpactForceMatrix;
    ImpactForceMatrix.setZero();

    // 각 정점과 충돌 지점 사이의 거리 계산
    TArray<float> Distances = {0, 0, 0};
    for (int index = 0; index < 3; index++)
    {
        Distances[index] = (TraignleVertices[index] - HitPoint).Size();
    }

    // 거리 기반 가중치 계산 및 힘 분배
    float WeightSum = 0.f;
    for (int i = 0; i < 3; ++i)
    {
        // 거리에 반비례하는 가중치 (가까울수록 큰 힘)
        float Weight = 1/Distances[i] * (Distances[0]* Distances[1]* Distances[2])/(Distances[0]* Distances[1] + Distances[1]* Distances[2] + Distances[0]* Distances[2]);

        // 각 정점의 x, y, z 방향에 힘 분배
        ImpactForceMatrix(i * 3 + 0, 0) = ImpactForce.X * Weight;
        ImpactForceMatrix(i * 3 + 1, 0) = ImpactForce.Y * Weight;
        ImpactForceMatrix(i * 3 + 2, 0) = ImpactForce.Z * Weight;
        WeightSum += Weight;
    }
    UE_LOG(LogTemp, Warning, TEXT("Weigh Sum = %f"), WeightSum);
    return ImpactForceMatrix;
}

Matrix<float, 9, 9> UFEMCalculateComponent::SubKMatrix(const Matrix<float, 12, 12> KMatrix, const int32 ExcludedIndex)
{
    Matrix<float, 9, 9> SubMatrix;
    SubMatrix.setZero();

    // 고정점에 해당하는 행과 열을 제외한 9x9 부분 행렬 추출
    for (int i = 0; i < 9; i++)
    {
        for (int j = 0; j < 9; j++)
        {
            int TargetRow = i;
            int TargetCol = j;
            // 제외할 정점의 인덱스 이후의 요소는 3칸씩 건너뜀
            if (i >= (ExcludedIndex- 1) * 3)
            {
                TargetRow += 3;
            }
            if (j >= (ExcludedIndex-1) * 3)
            {
                TargetCol += 3;
            }
            SubMatrix(i, j) = KMatrix(TargetRow, TargetCol);
        }
    }
    return SubMatrix;
}

void UFEMCalculateComponent::SetUndeformedPositions()
{
    UndeformedPositions.SetNum(TetMeshVertices.Num() * 3); // x, y, z로 3배 크기

    for (int32 i = 0; i < TetMeshVertices.Num(); ++i)
    {
        UndeformedPositions[3 * i] = TetMeshVertices[i].X;
        UndeformedPositions[3 * i + 1] = TetMeshVertices[i].Y;
        UndeformedPositions[3 * i + 2] = TetMeshVertices[i].Z;
    }
}

void UFEMCalculateComponent::KMatrix()
{
    double StartTime = FPlatformTime::Seconds();

    if (bUseParallelComputation)
    {
        KMatrixParallel();
    }
    else
    {
        KMatrixSequential();
    }

    double EndTime = FPlatformTime::Seconds();
    KMatrixTimeMs = (EndTime - StartTime) * 1000.0;

    if (bEnableProfiling)
    {
        UE_LOG(LogTemp, Log, TEXT("[FEM Profiling] KMatrix %s: %.3f ms (%d tets)"),
            bUseParallelComputation ? TEXT("Parallel") : TEXT("Sequential"),
            KMatrixTimeMs,
            Tets.Num());
    }
}

Matrix<float, 12, 12> UFEMCalculateComponent::ComputeKElementForTet(int32 TetIndex) const
{
    // 정점 인덱스 추출
    const FIntVector4& Tet = Tets[TetIndex];
    const int32 VertexIndex[4] = { Tet.X, Tet.Y, Tet.Z, Tet.W };

    // 3x4 위치 행렬 구성
    Matrix<float, 3, 4> Demention;
    for (int vtx = 0; vtx < 4; vtx++)
    {
        for (int dim = 0; dim < 3; dim++)
        {
            Demention(dim, vtx) = UndeformedPositions[3 * VertexIndex[vtx] + dim];
        }
    }

    // Jacobian 행렬 계산 (로컬 → 전역 좌표 변환)
    const Matrix<float, 3, 3> Jaco = Jacobian(Demention);

    // 형상함수(Shape Function) 미분 행렬 구성 (상수)
    // 사면체의 4개 형상함수: N1 = 1-ξ-η-ζ, N2 = ξ, N3 = η, N4 = ζ
    static const Matrix<float, 4, 3> ShapeFunctionDiffMatrix = []()
    {
        Matrix<float, 4, 3> M;
        M.setZero();
        M(0, 0) = -1; M(0, 1) = -1; M(0, 2) = -1;  // ∂N1/∂ξ, ∂N1/∂η, ∂N1/∂ζ
        M(1, 0) =  1;                                 // ∂N2/∂ξ = 1
        M(2, 1) =  1;                                 // ∂N3/∂η = 1
        M(3, 2) =  1;                                 // ∂N4/∂ζ = 1
        return M;
    }();

    // 전역 좌표계에 대한 형상함수 미분: ∂N/∂x = ∂N/∂ξ * J^-1
    const Matrix<float, 4, 3> Result = ShapeFunctionDiffMatrix * Jaco.inverse();
    const float Volume = GetTetVolume(Jaco);

    // K = V * B^T * E * B
    const Matrix<float, 6, 12> MatrixB = BMatrix(Result);
    const Matrix<float, 6, 6>  MatrixE = EMatrix();
    return Volume * MatrixB.transpose() * MatrixE * MatrixB;
}

void UFEMCalculateComponent::KMatrixSequential()
{
    KElements.SetNum(Tets.Num());
    for (int32 i = 0; i < Tets.Num(); i++)
    {
        KElements[i] = ComputeKElementForTet(i);
    }
}

void UFEMCalculateComponent::KMatrixParallel()
{
    KElements.SetNum(Tets.Num());
    // 각 스레드가 서로 다른 인덱스에만 쓰므로 동기화 불필요
    ParallelFor(Tets.Num(), [&](int32 i)
    {
        KElements[i] = ComputeKElementForTet(i);
    });
}

Matrix<float, 6, 12> UFEMCalculateComponent::BMatrix(Matrix<float, 4, 3> M) const
{
    // B 행렬을 Eigen 초기화 리스트로 직접 구성
    // 각 행은 특정 변형률 성분에 대응
    Matrix<float, 6, 12> B;
    B <<
        // 행 1: εxx = ∂u/∂x
        M(0,0), 0, 0, M(1,0), 0, 0, M(2,0), 0, 0, M(3,0), 0, 0,
        // 행 2: εyy = ∂v/∂y
        0, M(0,1), 0, 0, M(1,1), 0, 0, M(2,1), 0, 0, M(3,1), 0,
        // 행 3: εzz = ∂w/∂z
        0, 0, M(0,2), 0, 0, M(1,2), 0, 0, M(2,2), 0, 0, M(3,2),
        // 행 4: γxy = ∂u/∂y + ∂v/∂x
        M(0,1), M(0,0), 0, M(1,1), M(1,0), 0, M(2,1), M(2,0), 0, M(3,1), M(3,0), 0,
        // 행 5: γyz = ∂v/∂z + ∂w/∂y
        0, M(0,2), M(0,1), 0, M(1,2), M(1,1), 0, M(2,2), M(2,1), 0, M(3,2), M(3,1),
        // 행 6: γzx = ∂w/∂x + ∂u/∂z
        M(0,2), 0, M(0,0), M(1,2), 0, M(1,0), M(2,2), 0, M(2,0), M(3,2), 0, M(3,0);

    return B;
}

Matrix<float, 3, 3> UFEMCalculateComponent::Jacobian(Matrix<float, 3, 4> PositionMatrix) const
{
    Matrix<float, 3, 3> Result;
    Result.setZero();

    // 첫 번째 정점을 기준으로 나머지 정점까지의 벡터 계산
    // 첫 번째 열: r1 - r0
    Result(0, 0) = PositionMatrix(0, 1) - PositionMatrix(0, 0);
    Result(1, 0) = PositionMatrix(1, 1) - PositionMatrix(1, 0);
    Result(2, 0) = PositionMatrix(2, 1) - PositionMatrix(2, 0);

    // 두 번째 열: r2 - r0
    Result(0, 1) = PositionMatrix(0, 2) - PositionMatrix(0, 0);
    Result(1, 1) = PositionMatrix(1, 2) - PositionMatrix(1, 0);
    Result(2, 1) = PositionMatrix(2, 2) - PositionMatrix(2, 0);

    // 세 번째 열: r3 - r0
    Result(0, 2) = PositionMatrix(0, 3) - PositionMatrix(0, 0);
    Result(1, 2) = PositionMatrix(1, 3) - PositionMatrix(1, 0);
    Result(2, 2) = PositionMatrix(2, 3) - PositionMatrix(2, 0);

    return Result / 100;
}

Matrix<float, 6, 6> UFEMCalculateComponent::EMatrix() const
{
    // E 행렬을 Eigen 초기화 리스트로 직접 구성
    Matrix<float, 6, 6> E;
    E <<
       // 수직 응력 성분
       Lambda + 2 * Mu, Lambda,          Lambda,           0,  0,  0,
       Lambda,          Lambda + 2 * Mu, Lambda,           0,  0,  0,
       Lambda,          Lambda,          Lambda + 2 * Mu,  0,  0,  0,
       // 전단 응력 성분 (μ = 전단 계수)
       0,               0,               0,                Mu, 0,  0,
       0,               0,               0,                0,  Mu, 0,
       0,               0,               0,                0,  0,  Mu;

    return E;
}

float UFEMCalculateComponent::GetTetVolume(Matrix<float, 3, 3> Jaco) const
{
    return Jaco.determinant() / 6;
}

FInt32Vector4 UFEMCalculateComponent::GetClosestTriangleAndTet(const FVector& HitPosition, int32& OutExcludedIndex)
{
    double StartTime = FPlatformTime::Seconds();

    FInt32Vector4 Result;
    if (bUseParallelComputation)
    {
        Result = GetClosestTriangleAndTetParallel(HitPosition, OutExcludedIndex);
    }
    else
    {
        Result = GetClosestTriangleAndTetSequential(HitPosition, OutExcludedIndex);
    }

    double EndTime = FPlatformTime::Seconds();
    SearchTimeMs = (EndTime - StartTime) * 1000.0;

    if (bEnableProfiling)
    {
        UE_LOG(LogTemp, Log, TEXT("[FEM Profiling] GetClosestTriangleAndTet %s: %.3f ms (%d tets)"),
            bUseParallelComputation ? TEXT("Parallel") : TEXT("Sequential"),
            SearchTimeMs,
            Tets.Num());
    }

    return Result;
}

UFEMCalculateComponent::FTriangleSearchResult UFEMCalculateComponent::ComputeLocalMinForTet(int32 TetIndex, const FVector& HitPosition) const
{
    const FIntVector4& Tet = Tets[TetIndex];
    const FVector A = TetMeshVertices[Tet.X];
    const FVector B = TetMeshVertices[Tet.Y];
    const FVector C = TetMeshVertices[Tet.Z];
    const FVector D = TetMeshVertices[Tet.W];

    // 사면체의 4개 표면 삼각형에 대해 거리 계산 (ABC, ABD, ACD, BCD)
    const float Distances[4] = {
        DistanceToTriangle(HitPosition, A, B, C),
        DistanceToTriangle(HitPosition, A, B, D),
        DistanceToTriangle(HitPosition, A, C, D),
        DistanceToTriangle(HitPosition, B, C, D)
    };

    int32 MinIndex = 0;
    float MinDist = Distances[0];
    for (int32 j = 1; j < 4; ++j)
    {
        if (Distances[j] < MinDist)
        {
            MinDist = Distances[j];
            MinIndex = j;
        }
    }

    return { MinDist, TetIndex, MinIndex };
}

FInt32Vector4 UFEMCalculateComponent::BuildSearchResult(const FTriangleSearchResult& SR, int32& OutExcludedIndex)
{
    // 삼각형 인덱스 → [Vertex1, Vertex2, Vertex3] 매핑
    // 0: ABC (D 제외), 1: ABD (C 제외), 2: ACD (B 제외), 3: BCD (A 제외)
    static constexpr int32 TriangleVertices[4][3] = {
        {1, 2, 3}, {1, 2, 4}, {1, 3, 4}, {2, 3, 4}
    };
    static constexpr int32 ExcludedIndices[4] = {4, 3, 2, 1};

    FInt32Vector4 Result;
    Result[0] = SR.TetIndex;
    Result[1] = TriangleVertices[SR.TriIndex][0];
    Result[2] = TriangleVertices[SR.TriIndex][1];
    Result[3] = TriangleVertices[SR.TriIndex][2];
    OutExcludedIndex = ExcludedIndices[SR.TriIndex];
    return Result;
}

FInt32Vector4 UFEMCalculateComponent::GetClosestTriangleAndTetSequential(const FVector& HitPosition, int32& OutExcludedIndex)
{
    FTriangleSearchResult Best;

    for (int i = 0; i < Tets.Num(); ++i)
    {
        FTriangleSearchResult Local = ComputeLocalMinForTet(i, HitPosition);
        if (Local.MinDistance < Best.MinDistance)
        {
            Best = Local;
        }
    }
    return BuildSearchResult(Best, OutExcludedIndex);
}

FInt32Vector4 UFEMCalculateComponent::GetClosestTriangleAndTetParallel(const FVector& HitPosition, int32& OutExcludedIndex)
{
    // [동기화 전략: Double-Checked Locking]
    // atomic으로 1차 필터링 → Lock은 실제 갱신 시에만 진입
    std::atomic<float> GlobalMinDistance(FLT_MAX);
    FCriticalSection Mutex;
    FTriangleSearchResult GlobalBest;

    ParallelFor(Tets.Num(), [&](int32 i)
    {
        FTriangleSearchResult Local = ComputeLocalMinForTet(i, HitPosition);

        // 1차 검사 (Lock-Free): 대부분의 스레드는 여기서 걸러짐
        if (Local.MinDistance < GlobalMinDistance.load())
        {
            FScopeLock Lock(&Mutex);
            // 2차 검사 (Inside Lock): 다른 스레드가 이미 더 작은 값을 설정했을 수 있음
            if (Local.MinDistance < GlobalMinDistance.load())
            {
                GlobalMinDistance.store(Local.MinDistance);
                GlobalBest = Local;
            }
        }
    });

    return BuildSearchResult(GlobalBest, OutExcludedIndex);
}

FInt32Vector4 UFEMCalculateComponent::GetClosestTriangleAndTetParallel_Mutex(
    const FVector& HitPosition, int32& OutExcludedIndex)
{
    // 단일 Lock으로 모든 공유 데이터 보호 (단순하지만 경합 발생 가능)
    FCriticalSection Mutex;
    FTriangleSearchResult GlobalBest;

    ParallelFor(Tets.Num(), [&](int32 i)
    {
        FTriangleSearchResult Local = ComputeLocalMinForTet(i, HitPosition);

        FScopeLock Lock(&Mutex);
        if (Local.MinDistance < GlobalBest.MinDistance)
        {
            GlobalBest = Local;
        }
    });

    return BuildSearchResult(GlobalBest, OutExcludedIndex);
}

FInt32Vector4 UFEMCalculateComponent::GetClosestTriangleAndTetParallel_LockFree(const FVector& HitPosition, int32& OutExcludedIndex)
{
    // Worker 수만큼 슬롯을 미리 확보하고, 각 Worker가 담당 범위를 직접 순회
    const int32 NumWorkers = FTaskGraphInterface::Get().GetNumWorkerThreads();
    const int32 NumTets    = Tets.Num();
    const int32 ChunkSize  = FMath::DivideAndRoundUp(NumTets, NumWorkers);

    TArray<FTriangleSearchResult> ThreadResults;
    ThreadResults.AddDefaulted(NumWorkers);

    // ParallelFor의 인덱스 = WorkerIndex (0 ~ NumWorkers-1)
    // 각 Worker는 ThreadResults[WorkerIndex] 슬롯에만 접근 → 동기화 불필요
    ParallelFor(NumWorkers, [&](int32 WorkerIndex)
    {
        const int32 Begin = WorkerIndex * ChunkSize;
        const int32 End   = FMath::Min(Begin + ChunkSize, NumTets);

        FTriangleSearchResult& LocalBest = ThreadResults[WorkerIndex];
        for (int32 i = Begin; i < End; ++i)
        {
            FTriangleSearchResult Local = ComputeLocalMinForTet(i, HitPosition);
            if (Local.MinDistance < LocalBest.MinDistance)
            {
                LocalBest = Local;
            }
        }
    });

    // Reduction: 메인 스레드에서 모든 슬롯 취합
    FTriangleSearchResult Final;
    for (const FTriangleSearchResult& R : ThreadResults)
    {
        if (R.MinDistance < Final.MinDistance)
        {
            Final = R;
        }
    }

    if (Final.TetIndex == -1)
    {
        OutExcludedIndex = -1;
        return FInt32Vector4{ -1, 0, 0, 0 };
    }

    return BuildSearchResult(Final, OutExcludedIndex);
}

float UFEMCalculateComponent::DistanceToTriangle(const FVector& OtherPoint, const FVector& PointA, const FVector& PointB, const FVector& PointC) const
{
    // 삼각형의 두 에지 벡터
    FVector AB = PointB - PointA;
    FVector AC = PointC - PointA;

    // 법선 벡터: n = AB × AC
    FVector Normal = FVector::CrossProduct(AB, AC).GetSafeNormal();

    // 점-평면 거리: |n · (p - a)|
    return FMath::Abs(FVector::DotProduct((OtherPoint - PointA), Normal));
}
