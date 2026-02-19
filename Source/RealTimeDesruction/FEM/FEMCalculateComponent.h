// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../WeightedGraph/WeightedGraph.h"
#include <Eigen>
#include <shared_mutex>
#include <atomic>
#include "Async/ParallelFor.h"
#include "HAL/CriticalSection.h"

#include "FEMCalculateComponent.generated.h"

using namespace Eigen;

class UStaticMeshComponent;
class UTetMeshGenerateComponent;

/*
 * ==================================================================================
 * FEM 기반 실시간 파괴 시스템 개요 (Weighted Centroidal Voronoi Tessellation)
 * ==================================================================================
 *
 * 본 컴포넌트는 유한요소법(Finite Element Method, FEM)을 사용하여 충돌 시
 * 물체 내부의 에너지를 계산하고, 이를 기반으로 물체를 분할하는 시스템입니다.
 *
 * == 전체 프로세스 ==
 *
 * 1. 초기화 단계 (BeginPlay)
 *    - Static Mesh로부터 표면 정점과 삼각형 추출
 *    - TetWild를 사용하여 내부를 채우는 사면체(Tetrahedron) 메쉬 생성
 *    - 사면체들로부터 가중치 그래프(Weighted Graph) 구축
 *
 * 2. 변형되지 않은 위치 설정 (SetUndeformedPositions)
 *    - 각 정점의 초기 위치를 저장하여 변형 전 기준 좌표계 생성
 *
 * 3. 강성 행렬 계산 (KMatrix)
 *    - 각 사면체마다 12x12 강성 행렬(Stiffness Matrix) 계산
 *    - K = V * B^T * E * B 공식 사용
 *    - B: 변형률-변위 행렬 (Strain-Displacement Matrix)
 *    - E: 탄성 행렬 (Elasticity Matrix, Lamé 파라미터 사용)
 *    - V: 사면체 부피
 *
 * 4. 충돌 에너지 계산 (CalculateEnergyAtTatUsingFEM)
 *    a) 충돌 지점에서 가장 가까운 사면체와 삼각형 면 찾기
 *    b) 충격력 분배: 거리 기반 가중치로 삼각형의 세 정점에 분배
 *    c) 변위 계산: Ku = F 선형 시스템 풀이 (K: 강성행렬, u: 변위, F: 힘)
 *    d) 변형 구배(Deformation Gradient) 계산: G = Ds * Dm^-1
 *       - Dm: 변형 전 위치 행렬
 *       - Ds: 변형 후 위치 행렬 = Dm + u
 *    e) 변형률 텐서(Strain Tensor) 계산: ε = (G + G^T)/2 - I
 *    f) 에너지 밀도 계산: ψ = μ(ε:ε) + (λ/2)(tr(ε))²
 *       - μ, λ: Lamé 파라미터 (재료의 탄성 특성)
 *       - ε:ε: 변형률 텐서의 이중 내적
 *       - tr(ε): 변형률 텐서의 대각합
 *    g) 총 에너지: E = ψ * V
 *
 * 5. 가중치 그래프 활용
 *    - 각 사면체의 에너지 값을 그래프의 에지 가중치로 설정
 *    - Weighted Voronoi Tessellation을 통해 물체 분할 영역 결정
 *
 * == 주요 수학적 개념 ==
 *
 * - Jacobian 행렬: 로컬 좌표계에서 전역 좌표계로의 변환
 * - B 행렬: 형상함수(Shape Function)의 미분으로 구성, 변형률과 변위 연결
 * - E 행렬: 응력-변형률 관계를 정의하는 재료 특성 행렬
 * - 선형 탄성 모델: 작은 변형에서 응력과 변형률이 선형 관계
 *
 * == 재료 특성 파라미터 ==
 *
 * - Lambda (λ): 첫 번째 Lamé 파라미터, 부피 변화 저항
 * - Mu (μ): 두 번째 Lamé 파라미터 (전단 계수), 형상 변화 저항
 *
 * == 병렬 처리 ==
 *
 * - bUseParallelComputation: 강성 행렬 계산 및 가장 가까운 삼각형 탐색을
 *   병렬로 처리하여 성능 향상
 *
 * ==================================================================================
 */

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class REALTIMEDESRUCTION_API UFEMCalculateComponent : public UActorComponent
{
	GENERATED_BODY()
	friend UTetMeshGenerateComponent;

public:	

	UFEMCalculateComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Lambda;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Mu;

	// Multithreading Performance Settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
	bool bUseParallelComputation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
	bool bEnableProfiling = false;

	// Profiling data structure
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Performance")
	double KMatrixTimeMs = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Performance")
	double SearchTimeMs = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Performance")
	double SequentialSearchTimeMs = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Performance")
	double ParallelSearchTimeMs = 0.0;

	/**
	 * FEM을 사용하여 충돌 지점에서의 변형 에너지를 계산
	 *
	 * @param Velocity - 충돌 전 속도
	 * @param NextTickVelocity - 충돌 후 속도
	 * @param Mass - 충돌 물체의 질량
	 * @param HitPoint - 충돌 지점 좌표
	 * @return 계산된 변형 에너지 값
	 *
	 * 프로세스:
	 * 1. 충돌 지점에서 가장 가까운 사면체와 표면 삼각형 찾기
	 * 2. 충격력을 삼각형의 세 정점에 거리 기반으로 분배
	 * 3. Ku = F 시스템을 풀어 변위(u) 계산
	 * 4. 변위로부터 변형 구배 및 변형률 텐서 계산
	 * 5. 에너지 밀도 공식으로 최종 에너지 산출
	 */
	UFUNCTION(BlueprintCallable)
	float CalculateEnergyAtTatUsingFEM(const FVector& Velocity, const FVector& NextTickVelocity, const float Mass, const FVector& HitPoint);
	
	//! Energy at which to stop optimizing tet quality and accept the result.
	UPROPERTY(EditAnywhere, Category = "Dataflow", meta = (ClampMin = "0.0"))
	double IdealEdgeLength = 0.05;

	//! Maximum number of optimization iterations.
	UPROPERTY(EditAnywhere, Category = "Dataflow", meta = (ClampMin = "1"))
	int32 MaxIterations = 80;

	//! Energy at which to stop optimizing tet quality and accept the result.
	UPROPERTY(EditAnywhere, Category = "Dataflow", meta = (ClampMin = "0.0"))
	double StopEnergy = 10;

	//! Relative tolerance, controlling how closely the mesh must follow the input surface.z`
	UPROPERTY(EditAnywhere, Category = "Dataflow", meta = (ClampMin = "0.0"))
	double EpsRel = 1e-3;

	//! Coarsen the tet mesh result.
	UPROPERTY(EditAnywhere, Category = "Dataflow", meta = ())
	bool bCoarsen = true;

	//! Enforce that the output boundary surface should be manifold.
	UPROPERTY(EditAnywhere, Category = "Dataflow", meta = ())
	bool bExtractManifoldBoundarySurface = false;

	//! Skip the initial simplification step.
	UPROPERTY(EditAnywhere, Category = "Dataflow", meta = ())
	bool bSkipSimplification = false;

	//! Invert tetrahedra.
	UPROPERTY(EditAnywhere, Category = "Dataflow", meta = ())
	bool bInvertOutputTets = false;

	// 전체 정점 위치 배열
	TArray<FVector> TetMeshVertices;

	// 사면체 4개 정점의 인덱스 배열
	TArray<FIntVector4> Tets;

	WeightedGraph Graph{ false };

	TArray<uint32> CurrentImpactPoint;

protected:

	virtual void BeginPlay() override;

private:

	/**
	 * 사면체 메쉬 초기화 및 FEM 전처리
	 * 1. Static Mesh로부터 표면 정점과 삼각형 추출
	 * 2. TetWild 라이브러리를 사용하여 내부를 채우는 사면체 메쉬 생성
	 * 3. 변형되지 않은 초기 위치 저장
	 * 4. 모든 사면체의 강성 행렬 계산
	 * 5. 가중치 그래프 생성
	 */
	void InitializeTetMesh();

	/**
	 * 사면체 메쉬로부터 가중치 그래프 생성
	 *
	 * 각 사면체의 6개 에지를 그래프에 추가하여 연결 구조 생성
	 * 이 그래프는 나중에 Weighted Voronoi Tessellation에서
	 * 에너지 기반 가중치를 설정하여 물체 분할 영역을 결정하는데 사용됨
	 */
	void GenerateGraphFromTets();

	/**
	 * Sequential과 Parallel 검색 성능을 비교
	 *
	 * 동일한 HitPosition에 대해 두 방법으로 검색을 수행하고
	 * 성능과 결과의 일치 여부를 확인
	 */
	void BenchmarkSearchPerformance();

	/** 변형되지 않은 초기 정점 위치 배열 (x,y,z 순서로 연속 저장) */
	TArray<float> UndeformedPositions;

	/** 각 사면체의 12x12 강성 행렬(Stiffness Matrix) 배열 */
	TArray<Matrix<float, 12, 12>> KElements;

	/**
	 * 사면체의 변형 에너지를 계산
	 *
	 * @param DmMatrix - 변형 전 위치 행렬 (4x4)
	 * @param UMatrix - 변위 행렬 (4x4)
	 * @param TetVolume - 사면체 부피
	 * @return 에너지 값
	 *
	 * 공식: E = V * [μ(ε:ε) + (λ/2)(tr(ε))²]
	 * - 변형 구배: G = (Dm + u) * Dm^-1
	 * - 변형률 텐서: ε = (G + G^T)/2 - I
	 * - 에너지 밀도: ψ = μ(ε:ε) + (λ/2)(tr(ε))²
	*  * 프로세스:
	 * 1. 변형 구배(Deformation Gradient) G 계산
	 * 2. 변형률 텐서(Strain Tensor) ε 계산
	 * 3. 에너지 밀도 ψ 계산 (Lamé 파라미터 사용)
	 * 4. 부피를 곱하여 총 에너지 산출
	 */
	float CalculateEnergy(Matrix<float, 4, 4> DmMatrix, Matrix<float, 4, 4> UMatrix, float TetVolume) const;

	/**
	 * Ku = F 선형 시스템을 풀어 변위 행렬 계산
	 *
	 * @param KMatrix - 9x9 축소된 강성 행렬
	 * @param FMatrix - 9x1 힘 벡터
	 * @param TriangleIndex - 삼각형을 구성하는 정점 인덱스
	 * @param ExcludedIndex - 제외된 정점 인덱스 (고정점)
	 * @return 4x4 변위 행렬 (네 번째 행은 동차 좌표를 위한 1)
	 *
	 * 한 정점을 고정하고 나머지 세 정점의 변위를 계산
	 * 강성 행렬 K와 힘 벡터 F를 사용하여 변위 u를 계산
	 * u = K^-1 * F
	 *
	 * 한 정점을 고정(ExcludedIndex)하여 강체 이동을 제거하고
	 * 9개 자유도(3개 정점 × 3 방향)에 대한 변위를 계산
	 */
	Matrix<float, 4, 4> UMatrix(Matrix<float, 9, 9> KMatrix, Matrix<float, 9, 1> FMatrix, const FInt32Vector3 TriangleIndex, int32 ExcludedIndex);

	/**
	 * 충격력을 삼각형의 세 정점에 분배하여 힘 벡터 생성
	 *
	 * @param InitialVelocity - 충돌 전 속도
	 * @param NextTickVelocity - 충돌 후 속도
	 * @param Mass - 물체 질량
	 * @param HitPoint - 충돌 지점
	 * @param TraignleVertices - 삼각형의 세 정점
	 * @return 9x1 힘 벡터 (각 정점의 x,y,z 힘 성분)
	 *
	 * 충격력 F = m * Δv / Δt 를 계산 후
	 * 충돌 지점으로부터의 거리에 반비례하도록 세 정점에 분배
	 * 충격력을 삼각형의 세 정점에 분배
	 *
	 * 충돌 지점으로부터의 거리에 반비례하도록 가중치를 계산하여
	 * 총 충격력을 세 정점에 분산시킴
	 *
	 * 공식: F = m * Δv / Δt
	 * 가중치: w_i ∝ 1/d_i (거리에 반비례)
	 */
	Matrix<float, 9, 1> CalculateImpactForceMatrix(
		const FVector& InitialVelocity,
		const FVector& NextTickVelocity,
		const float Mass,
		const FVector& HitPoint,
		const TArray<FVector>& TraignleVertices
	);

	/**
	 * 변형되지 않은 초기 정점 위치를 1D 배열로 저장
	 *
	 * 각 정점의 x, y, z 좌표를 [x0, y0, z0, x1, y1, z1, ...] 형태로
	 * 연속된 배열에 저장하여 변형 전 기준 좌표계(Reference Configuration) 생성
	 * 이는 나중에 변형 구배와 변형률 계산의 기준이 됨
	 */
	void SetUndeformedPositions();

	/**
	 * 12x12 강성 행렬에서 한 정점을 제외한 9x9 부분 행렬 추출
	 *
	 * @param KMatrix - 원본 12x12 강성 행렬
	 * @param ExcludedIndex - 제외할 정점 인덱스 (1-4)
	 * @return 9x9 축소된 강성 행렬
	 *
	 * 한 정점을 고정점으로 설정하여 시스템의 강체 이동(Rigid Body Motion) 제거
	 * 
	 * 12x12 강성 행렬에서 한 정점을 제외한 9x9 부분 행렬 추출
	 *
	 * 사면체는 4개 정점 × 3 방향 = 12 자유도를 가짐
	 * 한 정점을 고정(ExcludedIndex)하면 9 자유도만 남음
	 * 이는 시스템의 강체 이동(Rigid Body Motion)을 제거하여
	 * 선형 시스템이 유일해를 가지도록 함
	*/
	Matrix<float, 9, 9>SubKMatrix(const Matrix<float, 12, 12> KMatrix, const int32 ExcludedIndex);

	/**
	 * 순차적으로 강성 행렬 계산
	 *
	 * 각 사면체에 대해 12x12 강성 행렬을 계산
	 * 공식: K = V * B^T * E * B
	 * - V: 사면체 부피
	 * - B: 변형률-변위 행렬 (6x12)
	 * - E: 탄성 행렬 (6x6, Lamé 파라미터 사용)
	 */
	void KMatrix();

	/** 순차적으로 강성 행렬 계산 */
	void KMatrixSequential();

	/** 병렬로 강성 행렬 계산 (성능 향상) */
	void KMatrixParallel();

	/**
	 * B 행렬 (변형률-변위 행렬, Strain-Displacement Matrix) 생성
	 *
	 * @param Matrix - 형상함수 미분 행렬 (4x3)
	 * @return 6x12 B 행렬
	 *
	 * B 행렬(변형률-변위 행렬, Strain-Displacement Matrix) 생성
	 *
	 * B 행렬은 정점의 변위로부터 변형률 성분을 계산하는 연산자
	 * ε = B * u (변형률 = B 행렬 × 변위)
	 *
	 * 6개 행: 변형률 성분 (εxx, εyy, εzz, γxy, γyz, γzx)
	 * 12개 열: 4개 정점 × 3 방향(x, y, z)
	 *
	 * 입력은 형상함수의 전역 좌표 미분 행렬 (∂Ni/∂x, ∂Ni/∂y, ∂Ni/∂z)
	 */
	Matrix<float, 6, 12> BMatrix(Matrix<float, 4, 3> Matrix);

	/**
	 * Jacobian 행렬 계산
	 *
	 * @param PositionMatrix - 3x4 정점 위치 행렬
	 * @return 3x3 Jacobian 행렬
	 *
	 * 로컬 좌표계(ξ, η, ζ)에서 전역 좌표계(x, y, z)로의 변환을 정의
	 * J = [∂x/∂ξ  ∂x/∂η  ∂x/∂ζ]
	 *     [∂y/∂ξ  ∂y/∂η  ∂y/∂ζ]
	 *     [∂z/∂ξ  ∂z/∂η  ∂z/∂ζ]
	 *
	 * 사면체의 경우 J = [r1-r0, r2-r0, r3-r0]
	 * 여기서 ri는 i번째 정점의 위치 벡터
	 *
	 * Jacobian의 행렬식은 사면체 부피와 직접 관련
	 */
	Matrix<float, 3, 3> Jacobian(Matrix<float, 3, 4> PositionMatrix);

	/**
	 * E 행렬 (탄성 행렬, Elasticity Matrix) 생성
	 *
	 * @return 6x6 탄성 행렬
	 *
	 * 선형 탄성 등방성 재료의 응력-변형률 관계를 정의
	 * σ = E * ε (응력 = 탄성 행렬 × 변형률)
	 *
	 * Lamé 파라미터(λ, μ)를 사용한 구성 방정식:
	 * - 대각 블록: 수직 응력-변형률 관계
	 * - 비대각 요소: Poisson 효과 (한 방향 변형이 다른 방향에 미치는 영향)
	 * - 하단 대각: 전단 응력-변형률 관계
	 */
	Matrix<float, 6, 6> EMatrix();

	/**
	 * Jacobian 행렬로부터 사면체 부피 계산
	 *
	 * @param Jaco - 3x3 Jacobian 행렬
	 * @return 사면체 부피
	 *
	 * 공식: V = |det(J)| / 6
	 */
	float GetTetVolume(Matrix<float, 3, 3> Jaco);

	/**
	 * 주어진 위치에서 가장 가까운 삼각형 면과 사면체 찾기
	 *
	 * @param HitPosition - 충돌 지점
	 * @param OutExcludedIndex - 찾은 삼각형에 포함되지 않는 정점 인덱스
	 * @return [사면체 인덱스, 정점1, 정점2, 정점3]
	 * 각 사면체의 4개 표면 삼각형과의 거리를 계산하여
	 * 전체 메쉬에서 가장 가까운 표면 찾기
	 */
	FInt32Vector4 GetClosestTriangleAndTet(const FVector& HitPosition, int32& OutExcludedIndex);

	/** 방법 1: 순차적 탐색 방식 */
	FInt32Vector4 GetClosestTriangleAndTetSequential(const FVector& HitPosition, int32& OutExcludedIndex);

	/** 방법 2: 병렬 탐색 방식 (Mutex 사용) */
	FInt32Vector4 GetClosestTriangleAndTetParallel(const FVector& HitPosition, int32& OutExcludedIndex);

	/** 방법 3: 병렬 탐색 방식 (Mutex 최적화 - 단일 Lock) */
	FInt32Vector4 GetClosestTriangleAndTetParallel_Mutex(const FVector& HitPosition, int32& OutExcludedIndex);

	/** 방법 4: 병렬 탐색 방식 (Lock-Free, 스레드별 로컬 결과) */
	FInt32Vector4 GetClosestTriangleAndTetParallel_LockFree(const FVector& HitPosition, int32& OutExcludedIndex);
	
	/**
	 * 점과 삼각형 평면 사이의 거리 계산
	 *
	 * @param OtherPoint - 대상 점
	 * @param PointA, PointB, PointC - 삼각형의 세 정점
	 * @return 수직 거리
	 *
	 * 삼각형의 법선 벡터를 계산하고 점-평면 거리 공식 사용
	 * 거리 = |n · (p - a)| / |n|
	 * 여기서 n은 법선 벡터, p는 대상 점, a는 삼각형의 한 정점
	 */
	float DistanceToTriangle(const FVector& OtherPoint, const FVector& PointA, const FVector& PointB, const FVector& PointC);
	
	template <typename T>
	void LogMatrix(const T& Matrix, const FString& MatrixName)
	{
		int Rows = Matrix.rows();
		int Cols = Matrix.cols();
		
		FString MatrixStr = FString::Printf(TEXT("%s Matrix (%d x %d): = \n"), *MatrixName, Rows, Cols);

		for (int i = 0; i < Rows; ++i)
		{
			FString RowStr;
			for (int j = 0; j < Cols; ++j)
			{
				RowStr += FString::Printf(TEXT("%f "), Matrix(i, j));
			}
			MatrixStr += RowStr + TEXT("\n");
		}
		
		UE_LOG(LogTemp, Log, TEXT("%s"), *MatrixStr);
	}
};
