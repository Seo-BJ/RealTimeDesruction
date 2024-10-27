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
	
    TestDistanceCalculation();
}

// Called every frame
void ATestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ATestActor::TestDistanceCalculation()
{
    // Create a weighted graph
    WeightedGraph MyGraph(false);

    UStaticMesh* MyMesh = MeshComponent->GetStaticMesh();

    if (MyMesh)
    {
        // Add vertices and links from the mesh
        AddVerticesAndLinksFromMesh(MyGraph);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get mesh from component."));
        return;
    }

    // Sources to start calculation
    TArray<uint32> Sources = { 0, 10, 20 };
    int k = 3;

    DistanceCalculate DistanceCalculator;

    TMap<uint32, DistOutEntry> Result = DistanceCalculator.Calculate(MyGraph, Sources, k);
    TArray<uint32> Vertex;
    UniqueVertex.GenerateValueArray(Vertex);

    for (const auto& v : Vertex)
    {
        UE_LOG(LogTemp, Log, TEXT("Vertex: %d, Distance: %f, Source: %d"), v, Result[v].Weight, Result[v].Source);
    }
}

void ATestActor::AddVerticesAndLinksFromMesh(WeightedGraph& Graph)
{
    if (!MeshComponent) return;

    // Get the mesh's static mesh
    UStaticMesh* Mesh = MeshComponent->GetStaticMesh();
    if (!Mesh) return;

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

        Graph.addLink(Index0, Index1, FVector(Vertex1 - Vertex0), 1.0f);
        Graph.addLink(Index1, Index2, FVector(Vertex2 - Vertex1), 1.0f);
        Graph.addLink(Index2, Index0, FVector(Vertex0 - Vertex2), 1.0f);
    }
}