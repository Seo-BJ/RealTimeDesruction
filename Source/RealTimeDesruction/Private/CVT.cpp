// Fill out your copyright notice in the Description page of Project Settings.

#include "CVT.h"

CVT::CVT()
{
}

CVT::~CVT()
{
}

// ���̵� �˰��� ����
void CVT::Lloyd_Algo(TArray<FVector>& InputSites)
{
    // �˰��� ���� �� �õ�����Ʈ TARRAY �غ�
    CVT::Sites = InputSites;
    TArray<FVector>& OldSites = InputSites;

    // �� �õ�����Ʈ�� ���� ��Ʈ����Ʈ�� ��ġ�Ҷ����� �ݺ�
    // ���γ��� �� �缳�� -> �� ���� �����߽� ���ϱ� -> �����߽� ���� ���ο� �õ�����Ʈ ����
    do
    {
        CVT::RefreshRegion();
        CVT::CalculateCentroids();
        OldSites = CVT::Sites;
        CVT::Sites = CVT::GenerateNewSite();
    } while (isEqualSites(CVT::Sites, OldSites));
}

// ����ƽ �޽� ������Ʈ���� ���ؽ� ���� �������� �Լ�
void CVT::GetVertexDataFromStaticMeshComponent(const UStaticMeshComponent* StaticMeshComponent)
{
    FPositionVertexBuffer* VertexBuffer = &StaticMeshComponent->GetStaticMesh()->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;
    // ���ؽ� ���۰� ��ȿ�ϸ� ��ȸ�ϸ� Vertices�� ���ؽ� ��ǥ �߰�
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

// ���γ��� �� �缳��
void CVT::RefreshRegion()
{
    // ���γ��� �� TATTAY �ʱ�ȭ
    CVT::Region.Empty();
    CVT::Region.AddUninitialized(CVT::Vertices.Num());

    // ���ؽ� TARRAY ��ȸ�ϸ� �� ���ؽ��� �õ� ����Ʈ TARRY�� ���° ���� ���� ������� Ȯ�� �� ����
    for (int32 i = 0; i < CVT::Vertices.Num(); ++i)
    {
        // ���� setup
        const FVector& Vertex = CVT::Vertices[i];
        int32 ClosestSiteIndex = -1;
        float ClosestDistance = FLT_MAX;

        // �õ� ����Ʈ TARRY ��ȸ�ϸ� ���° ���� ���� ������� Ȯ�� (��Ŭ���� �Ÿ�)
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

        // ���ؽ� TARRAY�� i��° ���ؽ��� �õ� ����Ʈ TARRY�� ���° ���� ���� ������� ����
        CVT::Region[i] = ClosestSiteIndex;
    }
}

// ���γ��� ���� �����߽��� ���
void CVT::CalculateCentroids()
{
    // ���� setup
    CVT::BaryCenters.Empty();
    CVT::BaryCenters.AddUninitialized(CVT::Sites.Num());

    TMap<int32, FVector> RegionSum;
    TMap<int32, int32> RegionCount;

    // ���ؽ� TARRAY ��ȸ�ϸ� �õ�����Ʈ���� ��ǥ���� ����, �׸��� ��� ���ߴ����� ���
    for (int32 i = 0; i < CVT::Vertices.Num(); ++i)
    {
        const FVector& Vertex = CVT::Vertices[i];
        int32 RegionIndex = CVT::Region[i];

        // �ش� ���� ���� ����� ó������ �ƴ����� ���� �ٸ��� ó��
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


    // ������ ��ǥ���� ����� ���� �����߽� ��ǥ�� ���
    for (const auto& Pair : RegionSum)
    {
        int32 RegionIndex = Pair.Key;
        const FVector& Sum = Pair.Value;
        int32 Count = RegionCount[RegionIndex];

        FVector Centroid = Sum / Count;
        CVT::BaryCenters[RegionIndex] = Centroid;
    }
}

// �����߽��� �������� ���ο� �õ� ����Ʈ ����
TArray<FVector> CVT::GenerateNewSite()
{
    // �� TARRAY ���� �� �ʱ�ȭ
    TArray<FVector> NewSites;
    NewSites.AddUninitialized(CVT::Sites.Num());

    // �����߽� TARRAY�� ��ȸ�ϸ� �õ�����Ʈ�� ����
    for (int32 i = 0; i < CVT::BaryCenters.Num(); ++i)
    {
        int32 ClosestSiteIndex = -1;
        float ClosestDistance = FLT_MAX;

        // �����߽ɿ��� ���� ����� ���ؽ��� �õ�����Ʈ�� ���� (��Ŭ���� �Ÿ�)
        for (int32 j = 0; j < CVT::Vertices.Num(); ++j)
        {
            // ���� ������ �ʴ� ���ؽ��� ����
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

// �� �õ� ����Ʈ�� ������ �������� �˻�
bool CVT::isEqualSites(TArray<FVector>& Sites1, TArray<FVector>& Sites2)
{
    for (int32 i = 0; i < Sites1.Num(); ++i)
    {
        if (!Sites1[i].Equals(Sites2[i]))
            return false;
    }
    return true;
}