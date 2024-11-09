// Fill out your copyright notice in the Description page of Project Settings.


#include "VoroTestComponent.h"

// For Voronoi Cells
static TMap<int32, FColor> ColorMap = {
	{0, FColor::Red},
	{1, FColor::Green},
	{2, FColor::Blue},
	{3, FColor::Yellow},
	{4, FColor::Cyan},
	{5, FColor::Magenta},
	{6, FColor::Turquoise},
	{7, FColor::Silver}
};

// Sets default values for this component's properties
UVoroTestComponent::UVoroTestComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UVoroTestComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	TetMeshComponent = GetOwner()->FindComponentByClass<UTetMeshGenerateComponent>();

	if (TetMeshComponent != nullptr)
	{
		GetOwner()->FindComponentByClass<UStaticMeshComponent>()->SetVisibility(bMeshVisibility);

		TetMeshComponent->Graph;

		Sites = getRandomVoronoiSites(TetMeshComponent->TetMeshVertices.Num());

		if (bUseCVT)
		{
			CVT CVT_inst;

			CVT_inst.SetVertices(TetMeshComponent->TetMeshVertices);
			CVT_inst.SetVoronoiSites(Sites);
			CVT_inst.Lloyd_Algo();
			Sites = CVT_inst.Sites;
			Region = CVT_inst.Region;
			VisualizeVertices();
		}

		// Gen Final Voro
		
	}
}


// Called every frame
void UVoroTestComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

TArray<uint32> UVoroTestComponent::getRandomVoronoiSites(const int32 VeticesSize)
{
	TArray<uint32> VoronoiSites;
	TSet<uint32> SelectedIndices;

	SiteNum = FMath::Min(SiteNum, VeticesSize);

	while (SelectedIndices.Num() < SiteNum)
	{
		// ���� �ε��� ����
		uint32 RandomIndex = FMath::RandRange(0, VeticesSize - 1);
		SelectedIndices.Add(RandomIndex);
	}
	VoronoiSites = SelectedIndices.Array();
	VoronoiSites.Sort();

	return VoronoiSites;
}

void UVoroTestComponent::VisualizeVertices()
{
	for (int32 i = 0; i < TetMeshComponent->TetMeshVertices.Num(); i++)
	{
		FVector WorldPosition = GetOwner()->GetActorTransform().TransformPosition(static_cast<FVector>(TetMeshComponent->TetMeshVertices[i]));
		int32 RegionOfVertex = Region[i];

		if (i == Sites[RegionOfVertex])
			DrawDebugPoint(GetWorld(), WorldPosition, 15.0f, FColor::Black, true, -1.0f, 0);
		else
			DrawDebugPoint(GetWorld(), WorldPosition, 15.0f, ColorMap[RegionOfVertex % ColorMap.Num()], true, -1.0f, 0);
	}
}