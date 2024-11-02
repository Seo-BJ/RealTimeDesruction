// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../DistanceCalculate/DistanceCalculate.h"
#include "Components/StaticMeshComponent.h"
#include "StaticMeshResources.h"
#include "Engine/StaticMesh.h"
#include "TestActor.generated.h"

UCLASS()
class REALTIMEDESRUCTION_API ATestActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATestActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	void DistanceCalculation();
	bool AddVerticesAndLinksFromMesh(WeightedGraph& Graph);

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* MeshComponent;
	TMap<uint32, DistOutEntry> Distance;
};
