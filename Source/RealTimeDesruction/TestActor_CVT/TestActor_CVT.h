// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../CVT/CVT.h"
#include "Components/StaticMeshComponent.h"
#include "StaticMeshResources.h"
#include "Engine/StaticMesh.h"
#include "DrawDebugHelpers.h"
#include "TestActor_CVT.generated.h"

UCLASS()
class REALTIMEDESRUCTION_API ATestActor_CVT : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATestActor_CVT();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	CVT CVT_inst;
	void VisualizeVertices();
	TArray<FVector3f> getRandomVoronoiSites(const TArray<FVector3f>& Vertices, int32 SiteNum);

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* MeshComponent;

	UPROPERTY(EditAnywhere)
	uint32 NumOfVoronoiSites;

	void ExecuteCVT();

	// 타이머 핸들
	FTimerHandle TimerHandle;

	// n초 지연 시간
	float DelayTime = 5.0f;
};
