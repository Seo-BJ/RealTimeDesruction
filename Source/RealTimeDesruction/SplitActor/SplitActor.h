#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "SplitActor.generated.h"

UCLASS()
class REALTIMEDESRUCTION_API ASplitActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ASplitActor();

	//void InitializeProceduralMesh(UProceduralMeshComponent* InMeshComponent);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProceduralMeshComponent* ProceduralMesh;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	void SetProceduralMesh(UProceduralMeshComponent* Mesh, TArray<UMaterialInterface*> Materials);
	void GenerateCollisionConvexMesh();
};