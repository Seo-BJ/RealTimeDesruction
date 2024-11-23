// Fill out your copyright notice in the Description page of Project Settings.


#include "SplitedActor.h"

// Sets default values
ASplitedActor::ASplitedActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
}

// Called when the game starts or when spawned
void ASplitedActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASplitedActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ASplitedActor::SetProceduralMesh(UProceduralMeshComponent* Mesh, UMaterialInterface* Material)
{
	ProceduralMesh = NewObject<UProceduralMeshComponent>(this);
	if (Mesh->GetProcMeshSection(0))
	{
		ProceduralMesh->SetProcMeshSection(0, *(Mesh->GetProcMeshSection(0)));

		ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		ProceduralMesh->SetMaterial(0, Material);

		ProceduralMesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		ProceduralMesh->RegisterComponent();
	}
	else
		UE_LOG(LogTemp, Warning, TEXT("Invalid Procedural Mesh!"));
}