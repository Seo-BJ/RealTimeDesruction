// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include <Eigen>

#include "FEMCalculateComponent.generated.h"

using namespace Eigen;

class UStaticMeshComponent;


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class REALTIMEDESRUCTION_API UFEMCalculateComponent : public UActorComponent
{
	GENERATED_BODY()

public:	

	UFEMCalculateComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaterialLambda;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaterialMu;

protected:

	virtual void BeginPlay() override;

private:

	// Algorithm 1: Make matrix Dm
	void CreateDm(const TArray<FVector>& Vertices, TArray<float>& undeformedPositions);

	// Algorithm 2: Make matrix K
	void BuildMatrixK(const TArray<float> UndeformedPositions, const TArray<FIntVector4> Tets, TArray<FMatrix>& KElements, float YoungsModulus, float PoissonsRatio);

	// Algorithm 3: Make matrix B
	Matrix<float, 6, 12> BuildMatrixB(FMatrix44f MinInverse);

	// Algorithm 4: Make matrix E
	Matrix<float, 6, 6> BuildMatrixE(float Lambda, float Mu);

	float GetTetraVolume(FIntVector4 Tetra, const TArray<float> UndeformedPositions);
	
	void ConvertArrayToEigenMatrix(const TArray64<float>& InArray, const int32 InRows, const int32 InColumns, Eigen::MatrixXf& OutMatrix);

	UFUNCTION(BlueprintCallable)
	TArray<FVector3f> GetVerticesFromStaticMesh(UStaticMeshComponent* MeshComponent);
};
