// Fill out your copyright notice in the Description page of Project Settings.

#include "CVT.h"

CVT::CVT()
{
}

CVT::~CVT()
{
}

// ���̵� �˰��� ����
void CVT::Lloyd_Algo()
{
    // �˰��� ���� �� �õ�����Ʈ TARRAY �غ�
    TArray<FVector3f> OldSites = CVT::Sites;

    // �� �õ�����Ʈ�� ���� ��Ʈ����Ʈ�� ��ġ�Ҷ����� �ݺ�
    // ���γ��� �� �缳�� -> �� ���� �����߽� ���ϱ� -> �����߽� ���� ���ο� �õ�����Ʈ ����
    do
    {
        CVT::CalculateCentroids();
        OldSites = CVT::Sites;
        CVT::Sites = CVT::GenerateNewSite();
        CVT::RefreshRegion();
        UE_LOG(LogTemp, Log, TEXT("DONE"))
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
        TSet<FVector3f> UniqueVertices;
        for (int32 Index = 0; Index < VertexCount; Index++)
        {
            const FVector3f vertex = VertexBuffer->VertexPosition(Index);
            UniqueVertices.Add(vertex);
        }
        CVT::Vertices = UniqueVertices.Array();
        UE_LOG(LogTemp, Display, TEXT("Vertices loading complete"))
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
        const FVector3f& Vertex = CVT::Vertices[i];
        int32 ClosestSiteIndex = -1;
        float ClosestDistance = FLT_MAX;

        // �õ� ����Ʈ TARRY ��ȸ�ϸ� ���° ���� ���� ������� Ȯ�� (��Ŭ���� �Ÿ�)
        for (int32 j = 0; j < CVT::Sites.Num(); ++j)
        {
            const FVector3f& Site = CVT::Sites[j];
            float Distance = FVector3f::DistSquared(Vertex, Site);

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

    TMap<int32, FVector3f> RegionSum;
    TMap<int32, int32> RegionCount;

    // ���ؽ� TARRAY ��ȸ�ϸ� �õ�����Ʈ���� ��ǥ���� ����, �׸��� ��� ���ߴ����� ���
    for (int32 i = 0; i < CVT::Vertices.Num(); ++i)
    {
        int32 RegionIndex = CVT::Region[i];

        // �ش� ���� ���� ����� ó������ �ƴ����� ���� �ٸ��� ó��
        if (RegionSum.Contains(RegionIndex))
        {
            RegionSum[RegionIndex] += CVT::Vertices[i];
            RegionCount[RegionIndex]++;
        }
        else
        {
            RegionSum.Add(RegionIndex, CVT::Vertices[i]);
            RegionCount.Add(RegionIndex, 1);
        }
    }

    // ������ ��ǥ���� ����� ���� �����߽� ��ǥ�� ���
    for (int32 i = 0; i < CVT::Sites.Num(); i++)
    {
        FVector3f Centroid = RegionSum[i] / RegionCount[i];
        UE_LOG(LogTemp, Log, TEXT("index : %d  centroid : x=%f, y=%f, z=%f"), i, Centroid.X, Centroid.Y, Centroid.Z);
        CVT::BaryCenters[i] = Centroid;
    }
}

// �����߽��� �������� ���ο� �õ� ����Ʈ ����
TArray<FVector3f> CVT::GenerateNewSite()
{
    // �� TARRAY ���� �� �ʱ�ȭ
    TArray<FVector3f> NewSites;
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

            float Distance = FVector3f::DistSquared(CVT::BaryCenters[i], CVT::Vertices[j]);

            if (Distance < ClosestDistance)
            {
                ClosestDistance = Distance;
                ClosestSiteIndex = j;
            }
        }
        NewSites[i] = CVT::Vertices[ClosestSiteIndex];
    }

    return NewSites;
}

// �� �õ� ����Ʈ�� ������ �������� �˻�
bool CVT::isEqualSites(TArray<FVector3f>& Sites1, TArray<FVector3f>& Sites2)
{
    for (int32 i = 0; i < Sites1.Num(); ++i)
    {
        if (!Sites1[i].Equals(Sites2[i]))
            return false;
    }
    return true;
}

void CVT::SetVoronoiSites(TArray<FVector3f> VoronoiSites)
{
    CVT::Sites = VoronoiSites;
    CVT::RefreshRegion();
}