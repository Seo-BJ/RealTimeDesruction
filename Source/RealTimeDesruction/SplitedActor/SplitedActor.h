// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "SplitedActor.generated.h"

UCLASS()
class REALTIMEDESRUCTION_API ASplitedActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASplitedActor();

	//void InitializeProceduralMesh(UProceduralMeshComponent* InMeshComponent);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProceduralMeshComponent* ProceduralMesh;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	void SetProceduralMesh(UProceduralMeshComponent* Mesh, UMaterialInterface* Material);
};
