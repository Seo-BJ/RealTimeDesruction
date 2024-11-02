// Fill out your copyright notice in the Description page of Project Settings.


#include "TestActor.h"

// Sets default values
ATestActor::ATestActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    RootComponent = MeshComponent;
}

// Called when the game starts or when spawned
void ATestActor::BeginPlay()
{
	Super::BeginPlay();
	
    if (!AddVerticesAndLinksFromMesh())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get mesh from component."));
    }
    else
    {
        TArray<uint32> Sources = Sources = { 0, 10, 20 };
        int k = 3;
        DistanceCalculation(Sources, k);
    }
}

// Called every frame
void ATestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ATestActor::DistanceCalculation(const TArray<uint32>& Sources, const int& k)
{
    DistanceCalculate DistanceCalculator;

    Distance = DistanceCalculator.Calculate(Graph, Sources, k);

    //for (const auto& r : Distance)
    //{
    //    UE_LOG(LogTemp, Log, TEXT("Vertex: %d, Distance: %f, Source: %d"), r.Key, r.Value.Weight, r.Value.Source);
    //}
}

bool ATestActor::AddVerticesAndLinksFromMesh()
{
    if (!MeshComponent) return false;
    
    // Get the mesh's static mesh
    UStaticMesh* Mesh = MeshComponent->GetStaticMesh();
    if (!Mesh) return false;

    const FStaticMeshLODResources& LODResources = Mesh->GetRenderData()->LODResources[0];
    const FPositionVertexBuffer& PositionVertexBuffer = LODResources.VertexBuffers.PositionVertexBuffer;
    const FRawStaticIndexBuffer& IndexBuffer = LODResources.IndexBuffer;

    for (int32 i = 0; i < IndexBuffer.GetNumIndices(); i += 3)
    {
        uint32 Index0 = IndexBuffer.GetIndex(i + 0);
        uint32 Index1 = IndexBuffer.GetIndex(i + 1);
        uint32 Index2 = IndexBuffer.GetIndex(i + 2);

        FVector3f Vertex0 = PositionVertexBuffer.VertexPosition(Index0);
        FVector3f Vertex1 = PositionVertexBuffer.VertexPosition(Index1);
        FVector3f Vertex2 = PositionVertexBuffer.VertexPosition(Index2);

        //UE_LOG(LogTemp, Log, TEXT("Vertex: %d, Position: %s"), Index0, *Vertex0.ToString());
        //UE_LOG(LogTemp, Log, TEXT("Vertex: %d, Position: %s"), Index1, *Vertex1.ToString());
        //UE_LOG(LogTemp, Log, TEXT("Vertex: %d, Position: %s"), Index2, *Vertex2.ToString());

        Graph.addLink(Index0, Index1, FVector(Vertex1 - Vertex0), 1.0);
        Graph.addLink(Index1, Index2, FVector(Vertex2 - Vertex1), 1.0);
        Graph.addLink(Index2, Index0, FVector(Vertex0 - Vertex2), 1.0);
    }

    return true;
}