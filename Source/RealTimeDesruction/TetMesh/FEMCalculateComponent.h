// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../WeightedGraph/WeightedGraph.h"
#include <Eigen>

#include "FEMCalculateComponent.generated.h"

using namespace Eigen;

class UStaticMeshComponent;
class UTetMeshGenerateComponent;

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

	UFUNCTION(BlueprintCallable)
	float CalculateEnergyAtTatUsingFEM(const FVector& Velocity, const FVector& NextTickVelocity, const float Mass, const FVector& HitPoint);

	// TetWild 매개변수

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

	TArray<FVector> TetMeshVertices;

	TArray<FIntVector4> Tets;

	WeightedGraph Graph{ false };

	TArray<uint32> CurrentImpactPoint;

protected:

	virtual void BeginPlay() override;

private:

	void InitializeTetMesh();

	void GenerateGraphFromTets();

	TArray<float> UndeformedPositions;

	TArray<Matrix<float, 12, 12>> KElements;

	// 에너지 계산
	float CalculateEnergy(Matrix<float, 4, 4> DmMatrix, Matrix<float, 4, 4> UMatrix, float TetVolume);

	// Ku = F 계산을 통해 변위 행렬 U를 계산
	Matrix<float, 4, 4> UMatrix(Matrix<float, 9, 9> KMatrix, Matrix<float, 9, 1> FMatrix, const FInt32Vector3 TriangleIndex, int32 ExcludedIndex);

	// Impact Force 행렬 계산
	Matrix<float, 9, 1> CalculateImpactForceMatrix(
		const FVector& InitialVelocity,
		const FVector& NextTickVelocity,
		const float Mass,
		const FVector& HitPoint,
		const TArray<FVector>& TraignleVertices
	);


	// Algorithm 1: Make matrix Dm
	void SetUndeformedPositions();

	Matrix<float, 9, 9>SubKMatrix(const Matrix<float, 12, 12> KMatrix, const int32 ExcludedIndex);

	// Algorithm 2: Make matrix K
	void KMatrix();

	// Algorithm 3: Make matrix B
	Matrix<float, 6, 12> BMatrix(Matrix<float, 4, 3> Matrix);

	Matrix<float, 3, 3> Jacobian(Matrix<float, 3, 4> PositionMatrix);



	// Algorithm 4: Make matrix E
	Matrix<float, 6, 6> EMatrix();

	float GetTetVolume(Matrix<float, 3, 3> Jaco);

	FInt32Vector4 GetClosestTriangleAndTet(const FVector& HitPosition, int32& OutExcludedIndex);
	
	float DistanceToTriangle(const FVector& OtherPoint, const FVector& PointA, const FVector& PointB, const FVector& PointC);

	void ConvertArrayToEigenMatrix(const TArray64<float>& InArray, const int32 InRows, const int32 InColumns, Eigen::MatrixXf& OutMatrix);

	UFUNCTION(BlueprintCallable)
	TArray<FVector3f> GetVerticesFromStaticMesh(UStaticMeshComponent* MeshComponent);


	template <typename T>
	void LogMatrix(const T& Matrix, const FString& MatrixName)
	{
		// 행렬의 크기 얻기
		int Rows = Matrix.rows();
		int Cols = Matrix.cols();

		// 로그 헤더 생성
		FString MatrixStr = FString::Printf(TEXT("%s Matrix (%d x %d): = \n"), *MatrixName, Rows, Cols);

		// 행렬의 각 원소를 출력
		for (int i = 0; i < Rows; ++i)
		{
			FString RowStr;
			for (int j = 0; j < Cols; ++j)
			{
				// 행렬의 원소를 문자열로 변환하여 RowStr에 추가
				RowStr += FString::Printf(TEXT("%f "), Matrix(i, j));
			}
			MatrixStr += RowStr + TEXT("\n");
		}

		// UE_LOG를 사용하여 출력
		UE_LOG(LogTemp, Log, TEXT("%s"), *MatrixStr);
	}

	/*
	UFUNCTION(BlueprintCallable, Category = "Mesh")
	FVector GetClosestPositionVertex(UStaticMeshComponent* StaticMeshComponent, FVector HitLocation);

	UFUNCTION(BlueprintCallable, Category = "Mesh")
	TArray<FVector> GetClosestTriangePositionAtHit(UStaticMeshComponent* StaticMeshComponent, const FHitResult& HitResult);
	*/

};
