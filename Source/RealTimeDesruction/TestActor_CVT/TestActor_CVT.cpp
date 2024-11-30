// Fill out your copyright notice in the Description page of Project Settings.

#include "TestActor_CVT.h"

// For Voronoi Cells
TMap<int32, FColor> ColorMap = {
    {0, FColor::Red},
    {1, FColor::Green},
    {2, FColor::Blue},
    {3, FColor::Yellow},
    {4, FColor::Cyan},
    {5, FColor::Magenta},
    {6, FColor::Turquoise},
    {7, FColor::Silver}
};

// Sets default values
ATestActor_CVT::ATestActor_CVT()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    NumOfVoronoiSites = 0;
}

// Called when the game starts or when spawned
void ATestActor_CVT::BeginPlay()
{
	Super::BeginPlay();

    // Test Code
	
    CVT_inst.GetVertexDataFromStaticMeshComponent(MeshComponent);
    CVT_inst.SetVoronoiSites(getRandomVoronoiSites(CVT_inst.Vertices, NumOfVoronoiSites));

    GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &ATestActor_CVT::ExecuteCVT, DelayTime, false);
    VisualizeVertices();
}

// Called every frame
void ATestActor_CVT::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);


}

void ATestActor_CVT::VisualizeVertices()
{
	for (int32 i = 0; i < CVT_inst.Vertices.Num(); i++)
	{
		FVector WorldPosition = GetActorTransform().TransformPosition(static_cast<FVector>(CVT_inst.Vertices[i]));
        int32 RegionOfVertex = CVT_inst.Region[i];

        if (i == CVT_inst.Sites[RegionOfVertex])
            DrawDebugPoint(GetWorld(), WorldPosition, 15.0f, FColor::White, true, -1.0f, 0);
        else
            DrawDebugPoint(GetWorld(), WorldPosition, 15.0f, ColorMap[RegionOfVertex % ColorMap.Num()], true, -1.0f, 0);
	}
}

TArray<uint32> ATestActor_CVT::getRandomVoronoiSites(const TArray<FVector>& Vertices, int32 SiteNum)
{
    TArray<uint32> VoronoiSites;
    TSet<uint32> SelectedIndices;

    SiteNum = FMath::Min(SiteNum, Vertices.Num());

    while (SelectedIndices.Num() < SiteNum)
    {
        // ���� �ε��� ����
        uint32 RandomIndex = FMath::RandRange(0, Vertices.Num() - 1);
        SelectedIndices.Add(RandomIndex);
    }
    VoronoiSites = SelectedIndices.Array();
    VoronoiSites.Sort();

    return VoronoiSites;
}

void ATestActor_CVT::ExecuteCVT()
{
    CVT_inst.Lloyd_Algo();
    VisualizeVertices();
}