#pragma once

#include "CoreMinimal.h"
#include "../DistanceCalculate/DistanceCalculate.h"
#include "ProceduralMeshComponent.h"
#include <shared_mutex>
#include "../TetMesh/FEMCalculateComponent.h"

// Not sure it is working...
class REALTIMEDESRUCTION_API SplitMesh
{
public:
	SplitMesh(const UStaticMesh* Mesh, const UFEMCalculateComponent* FEMComponent, const TMap<uint32, DistOutEntry>* Distance);
	~SplitMesh() {};
	FVector3f CalculateSplitPoint(const int32& p1, const int32& p2);
	TMap<uint32, TArray<FIntVector4>> SplitTetra(const FIntVector4& tetra);
	TMap<uint32, UProceduralMeshComponent*> Split();

private:
	void GenerateCombinations(const TArray<int32>& InputArray, TArray<int32>& CurrentCombination, int32 StartIndex, int32 CombinationSize, TArray<TArray<int32>>& Result);

	const UStaticMesh* Mesh;
	const UFEMCalculateComponent* FEMComponent;
	const FPositionVertexBuffer* PositionVertexBuffer;
	const FRawStaticIndexBuffer* IndexBuffer;
	const TMap<uint32, DistOutEntry>* Distance;
	const TArray<FIntVector4>* Tets;
	uint32 NumVertices;
	TArray<FVector3f> VerticesToAdd;
	TSet<uint32> Seed;
};