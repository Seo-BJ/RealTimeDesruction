// Fill out your copyright notice in the Description page of Project Settings.


#include "TetMeshGenerateComponent.h"

#include "FTetWildWrapper.h"

// Sets default values for this component's properties
UTetMeshGenerateComponent::UTetMeshGenerateComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


void UTetMeshGenerateComponent::BeginPlay()
{
	Super::BeginPlay();

	// StaticMesh�� ����
	TArray<FVector> Verts;
	TArray<FIntVector3> Tris;

	// Todo : Get Verts and Tris
	if (UStaticMeshComponent* StaticMeshComponent = GetOwner()->FindComponentByClass<UStaticMeshComponent>())
	{
		if (UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh())
		{
			// Access LOD 0 for simplicity, modify if you need other LODs
			const FStaticMeshLODResources& LODResource = StaticMesh->GetRenderData()->LODResources[0];

			// Get vertices
			const FPositionVertexBuffer& VertexBuffer = LODResource.VertexBuffers.PositionVertexBuffer;
			const int32 VertexCount = VertexBuffer.GetNumVertices();
			Verts.Reserve(VertexCount);
			for (int32 i = 0; i < VertexCount; i++)
			{
				double PositionX = VertexBuffer.VertexPosition(i).X;
				double PositionY = VertexBuffer.VertexPosition(i).Y	;
				double PositionZ = VertexBuffer.VertexPosition(i).Z;
				FVector PositionVector = FVector(PositionX, PositionY, PositionZ);
				Verts.Add(PositionVector);
			}

			// Get triangle indices
			const FIndexArrayView& IndexArray = LODResource.IndexBuffer.GetArrayView();
			const int32 IndexCount = IndexArray.Num();
			Tris.Reserve(IndexCount / 3);
			for (int32 i = 0; i < IndexCount; i += 3)
			{
				Tris.Add(FIntVector3(IndexArray[i], IndexArray[i + 1], IndexArray[i + 2]));
			}
		}
	}


	UE::Geometry::FTetWild::FTetMeshParameters Params;
	Params.bCoarsen = bCoarsen;
	Params.bExtractManifoldBoundarySurface = bExtractManifoldBoundarySurface;
	Params.bSkipSimplification = bSkipSimplification;

	Params.EpsRel = EpsRel;
	Params.MaxIts = MaxIterations;
	Params.StopEnergy = StopEnergy;
	Params.IdealEdgeLength = IdealEdgeLength;

	Params.bInvertOutputTets = bInvertOutputTets;

	FProgressCancel Progress;
	UE_LOG(LogTemp, Warning, TEXT("Generating tet mesh via TetWild..."));
	if (UE::Geometry::FTetWild::ComputeTetMesh(Params, Verts, Tris, TetMeshVertices, Tets, &Progress))
	{
		UE_LOG(LogTemp, Warning, TEXT("Succefully Generated tet mesh!"));

		UE_LOG(LogTemp, Display, TEXT("Vertex : %d"), TetMeshVertices.Num());
		UE_LOG(LogTemp, Display, TEXT("Tets : %d"), Tets.Num());

		/*
		for (int32 i = 0; i < TetMeshVertices.Num(); i++)
		{
			const FVector& Vertex = TetMeshVertices[i];
			UE_LOG(LogTemp, Display, TEXT("Vertex %d: X=%f, Y=%f, Z=%f"), i, Vertex.X, Vertex.Y, Vertex.Z);
		}
		// Logging Tets
		UE_LOG(LogTemp, Display, TEXT("Tets:"));
		for (int32 i = 0; i < Tets.Num(); i++)
		{
			const FIntVector4& Tet = Tets[i];
			UE_LOG(LogTemp, Display, TEXT("Tet %d: V0=%d, V1=%d, V2=%d, V3=%d"), i, Tet.X, Tet.Y, Tet.Z, Tet.W);
		}
		*/
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to Generate tet mesh!"));
	}
}
