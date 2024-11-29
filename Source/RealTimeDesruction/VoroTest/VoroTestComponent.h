// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../TetMesh/TetMeshGenerateComponent.h"
#include "../CVT/CVT.h"
#include "../DistanceCalculate/DistanceCalculate.h"
#include "../SplitMesh/SplitMesh.h"
#include "../SplitActor/SplitActor.h"
#include "Engine/StaticMeshActor.h"
#include "ProceduralMeshConversion.h"
#include "StaticMeshDescription.h"
#include "VoroTestComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class REALTIMEDESRUCTION_API UVoroTestComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UVoroTestComponent();

	UPROPERTY(EditAnywhere, Category = "Dataflow")
	bool bUseCVT;
	UPROPERTY(EditAnywhere, Category = "Dataflow")
	bool bMeshVisibility;
	UPROPERTY(EditAnywhere, Category = "Dataflow", meta = (ClampMin = "1"))
	int32 SiteNum = 10;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UTetMeshGenerateComponent* TetMeshComponent = nullptr;
	TArray<uint32> Sites;
	TArray<uint32> Region;

	TArray<uint32> getRandomVoronoiSites(const int32 VeticesSize);
	void VisualizeVertices();
	void DestroyActor(const UTetMeshGenerateComponent* TetComponent, const TMap<uint32, DistOutEntry>* Dist);
};
