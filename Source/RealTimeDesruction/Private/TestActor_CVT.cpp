// Fill out your copyright notice in the Description page of Project Settings.


#include "TestActor_CVT.h"

static TMap<int32, FColor> ColorMap = {
    {0, FColor::Red},
    {1, FColor::Green},
    {2, FColor::Blue},
    {3, FColor::Yellow},
    {4, FColor::Cyan},
    {5, FColor::Magenta}
};

// Sets default values
ATestActor_CVT::ATestActor_CVT()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
}

// Called when the game starts or when spawned
void ATestActor_CVT::BeginPlay()
{
	Super::BeginPlay();

    // Test Code
	
    CVT_inst.GetVertexDataFromStaticMeshComponent(MeshComponent);
    CVT_inst.SetVoronoiSites(getRandomVoronoiSites(CVT_inst.Vertices, 5));
    
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

        if (CVT_inst.Vertices[i].Equals(CVT_inst.Sites[RegionOfVertex]))
            DrawDebugPoint(GetWorld(), WorldPosition, 15.0f, FColor::White, true, -1.0f, 0);
        else
            DrawDebugPoint(GetWorld(), WorldPosition, 15.0f, ColorMap[RegionOfVertex%6], true, -1.0f, 0);            
	}
}

TArray<FVector3f> ATestActor_CVT::getRandomVoronoiSites(const TArray<FVector3f>& Vertices, int32 SiteNum)
{
    TArray<FVector3f> VoronoiSites;
    TSet<int32> SelectedIndices;

    SiteNum = FMath::Min(SiteNum, Vertices.Num());

    while (SelectedIndices.Num() < SiteNum)
    {
        // ·£´ý ÀÎµ¦½º »ý¼º
        int32 RandomIndex = FMath::RandRange(0, Vertices.Num() - 1);

        // Áßº¹µÈ ÀÎµ¦½º°¡ ¾Æ´Ò °æ¿ì ¼±ÅÃ
        if (!SelectedIndices.Contains(RandomIndex))
        {
            SelectedIndices.Add(RandomIndex);
            VoronoiSites.Add(Vertices[RandomIndex]);
        }
    }

    for (int32 i = 0; i < VoronoiSites.Num(); i++)
    {
        UE_LOG(LogTemp, Log, TEXT("Vertex %d: X=%f, Y=%f, Z=%f"), i, VoronoiSites[i].X, VoronoiSites[i].Y, VoronoiSites[i].Z);
    }

    return VoronoiSites;
}

void ATestActor_CVT::ExecuteCVT()
{
    CVT_inst.Lloyd_Algo();
    VisualizeVertices();
}