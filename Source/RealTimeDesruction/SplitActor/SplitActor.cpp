// Fill out your copyright notice in the Description page of Project Settings.


#include "SplitActor.h"

// Sets default values
ASplitActor::ASplitActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
}

// Called when the game starts or when spawned
void ASplitActor::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void ASplitActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ASplitActor::SetProceduralMesh(UProceduralMeshComponent* Mesh, TArray<UMaterialInterface*> Materials)
{
	ProceduralMesh = NewObject<UProceduralMeshComponent>(this);
	if (Mesh->GetProcMeshSection(0))
	{
		ProceduralMesh->SetProcMeshSection(0, *(Mesh->GetProcMeshSection(0)));

		ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		for (int i = 0; i < Materials.Num(); ++i)
			ProceduralMesh->SetMaterial(i, Materials[i]);

		ProceduralMesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		ProceduralMesh->RegisterComponent();
	}
	else
		UE_LOG(LogTemp, Warning, TEXT("Invalid Procedural Mesh!"));
}