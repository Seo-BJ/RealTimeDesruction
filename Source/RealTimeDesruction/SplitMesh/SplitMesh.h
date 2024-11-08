#pragma once

#include "CoreMinimal.h"
#include "../DistanceCalculate/DistanceCalculate.h"
#include "ProceduralMeshComponent.h"

// Not sure it is working...
class REALTIMEDESRUCTION_API SplitMesh
{
public:
	SplitMesh(const UStaticMesh* Mesh, const TMap<uint32, DistOutEntry>* Distance);
	~SplitMesh() {};
	FVector3f CalculateSplitPoint(const int32& p1, const int32& p2);
	TMap<uint32, FIntVector4> SplitTetra(const FIntVector4& tetra);
	TArray<UProceduralMeshComponent*> Split(const TArray<FIntVector4>& Tets);

private:
	const UStaticMesh* Mesh;
	const FPositionVertexBuffer* PositionVertexBuffer;
	const FRawStaticIndexBuffer* IndexBuffer;
	const TMap<uint32, DistOutEntry>* Distance;
	uint32 NumVertices;
	TArray<FVector3f> VerticesToAdd;
	TArray<FIntVector> IndexToAdd;
	TSet<uint32> Seed;
};