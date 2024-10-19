// Fill out your copyright notice in the Description page of Project Settings.

#include "CVT.h"

CVT::CVT()
{
}

CVT::~CVT()
{
}

void CVT::Lloyd_Algo(TArray<FVector>& InputSites)
{
    CVT::Sites = InputSites;
    TArray<FVector>& OldSites = InputSites;
    do
    {
        CVT::RefreshRegion();
        CVT::CalculateCentroids();
        OldSites = CVT::Sites;
        CVT::Sites = CVT::GenerateNewSite();
    } while (isEqualSite(CVT::Sites, OldSites));
}

void CVT::GetVertexDataFromStaticMeshComponent(const UStaticMeshComponent* StaticMeshComponent)
{
    FPositionVertexBuffer* VertexBuffer = &StaticMeshComponent->GetStaticMesh()->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;
    if (VertexBuffer)
    {
        const int32 VertexCount = VertexBuffer->GetNumVertices();
        for (int32 Index = 0; Index < VertexCount; Index++)
        {
            const FVector vertex = static_cast<FVector>(VertexBuffer->VertexPosition(Index));
            CVT::Vertices.Add(vertex);
        }
    }
}

void CVT::RefreshRegion()
{
    CVT::Region.Empty();
    CVT::Region.AddUninitialized(CVT::Vertices.Num());

    for (int32 i = 0; i < CVT::Vertices.Num(); ++i)
    {
        const FVector& Vertex = CVT::Vertices[i];
        int32 ClosestSiteIndex = -1;
        float ClosestDistance = FLT_MAX;

        for (int32 j = 0; j < CVT::Sites.Num(); ++j)
        {
            const FVector& Site = CVT::Sites[j];
            float Distance = FVector::DistSquared(Vertex, Site);

            if (Distance < ClosestDistance)
            {
                ClosestDistance = Distance;
                ClosestSiteIndex = j;
            }
        }

        CVT::Region[i] = ClosestSiteIndex;
    }
}

void CVT::CalculateCentroids()
{
    CVT::BaryCenters.Empty();
    CVT::BaryCenters.AddUninitialized(CVT::Sites.Num());

    TMap<int32, FVector> RegionSum;
    TMap<int32, int32> RegionCount;

    for (int32 i = 0; i < CVT::Vertices.Num(); ++i)
    {
        const FVector& Vertex = CVT::Vertices[i];
        int32 RegionIndex = CVT::Region[i];

        if (RegionSum.Contains(RegionIndex))
        {
            RegionSum[RegionIndex] += Vertex;
            RegionCount[RegionIndex]++;
        }
        else
        {
            RegionSum.Add(RegionIndex, Vertex);
            RegionCount.Add(RegionIndex, 1);
        }
    }

    for (const auto& Pair : RegionSum)
    {
        int32 RegionIndex = Pair.Key;
        const FVector& Sum = Pair.Value;
        int32 Count = RegionCount[RegionIndex];

        FVector Centroid = Sum / Count;
        CVT::BaryCenters[RegionIndex] = Centroid;
    }
}

TArray<FVector> CVT::GenerateNewSite()
{
    TArray<FVector> NewSites;
    NewSites.AddUninitialized(CVT::Sites.Num());

    for (int32 i = 0; i < CVT::BaryCenters.Num(); ++i)
    {
        int32 ClosestSiteIndex = -1;
        float ClosestDistance = FLT_MAX;

        for (int32 j = 0; j < CVT::Vertices.Num(); ++j)
        {
            if (CVT::Region[j] != i)
                continue;

            float Distance = FVector::DistSquared(CVT::BaryCenters[i], CVT::Vertices[j]);

            if (Distance < ClosestDistance)
            {
                ClosestDistance = Distance;
                ClosestSiteIndex = j;
            }
        }
        CVT::Sites[i] = CVT::Vertices[ClosestSiteIndex];
    }
    return NewSites;
}

bool CVT::isEqualSite(TArray<FVector>& Sites1, TArray<FVector>& Sites2)
{
    for (int32 i = 0; i < Sites1.Num(); ++i)
    {
        if (!Sites1[i].Equals(Sites2[i]))
            return false;
    }
    return true;
}