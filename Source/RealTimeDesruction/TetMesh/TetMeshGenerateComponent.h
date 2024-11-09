// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../WeightedGraph/WeightedGraph.h"
#include "TetMeshGenerateComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class REALTIMEDESRUCTION_API UTetMeshGenerateComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UTetMeshGenerateComponent();


	//
	// TetWild 매개변수
	//

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
		
	// UPROPERTY(EditAnywhere, Category = "Tet Mesh Info", meta = ())
	TArray<FVector> TetMeshVertices;

	// UPROPERTY(EditAnywhere, Category = "Tet Mesh Info", meta = ())
	TArray<FIntVector4> Tets;

	WeightedGraph Graph{ false };

protected:
	// Called when the game starts
	virtual void BeginPlay() override;


private:
	void GenerateGraphFromTets();
};
