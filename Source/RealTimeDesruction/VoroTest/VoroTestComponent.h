// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../TetMesh/FEMCalculateComponent.h"
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

	UFUNCTION(BlueprintCallable)
	void DestructMesh(const float Energy, const int32 SeedNum, const bool RandomSeed, const bool UseCVT);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFEMCalculateComponent* FEMComponent = nullptr;
	TArray<uint32> Seeds;
	TArray<uint32> Region;

	TArray<uint32> getVoronoiSeedByRandom(const int32 SeedNum);
	TArray<uint32> getVoronoiSeedByImpactPoint(const int32 SeedNum, const TArray<uint32> ImpactPoint);
	void VisualizeVertices();
	void DestroyActor(const TMap<uint32, DistOutEntry>* Dist);
	void UpdateGraphWeight(const float Energy, const TArray<uint32> ImpactPoint);
	
	template <typename T>
	TArray<uint32> getRandomElementsFromArray(const TArray<T>& InputArray, uint32 NumElements)
	{
		TArray<T> RandomElements;

		if (NumElements >= (uint32)InputArray.Num())
		{
			return InputArray;
		}

		// 원본 배열의 인덱스를 섞기 위한 리스트 생성
		TArray<int32> Indices;
		for (int32 i = 0; i < InputArray.Num(); ++i)
		{
			Indices.Add(i);
		}

		// 인덱스 배열을 섞기
		for (int32 i = Indices.Num() - 1; i > 0; --i)
		{
			int32 j = FMath::RandRange(0, i);
			Indices.Swap(i, j);
		}

		// 섞인 인덱스 중에서 NumElements만큼 선택
		for (int32 i = 0; i < (int32)NumElements; ++i)
		{
			RandomElements.Add(InputArray[Indices[i]]);
		}

		return RandomElements;
	}
};
