// Fill out your copyright notice in the Description page of Project Settings.

#include "CVT.h"

CVT::CVT()
{
}

CVT::~CVT()
{
}

// ���̵� �˰���� ����
void CVT::Lloyd_Algo()
{
    // �˰���� ���� �� �õ�����Ʈ TARRAY �غ�
    TArray<uint32> OldSites = CVT::Sites;

    // �� �õ�����Ʈ�� ���� ��Ʈ����Ʈ�� ��ġ�Ҷ����� �ݺ�
    // ���γ��� �� �缳�� -> �� ���� �����߽� ���ϱ� -> �����߽� ���� ���ο� �õ�����Ʈ ����
    do
    {
        CVT::CalculateCentroids();
        OldSites = CVT::Sites;
        CVT::Sites = CVT::GenerateNewSite();
        CVT::RefreshRegion();
        UE_LOG(LogTemp, Log, TEXT("Lloyd Algorithm is successfully executed."))
    } while (not isEqualSites(CVT::Sites, OldSites));
}

// ����ƽ �޽� ������Ʈ���� ���ؽ� ���� �������� �Լ�
void CVT::GetVertexDataFromStaticMeshComponent(const UStaticMeshComponent* StaticMeshComponent)
{
    FPositionVertexBuffer* VertexBuffer = &StaticMeshComponent->GetStaticMesh()->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;
    // ���ؽ� ���۰� ��ȿ�ϸ� ��ȸ�ϸ� Vertices�� ���ؽ� ��ǥ �߰�
    if (VertexBuffer)
    {
        const int32 VertexCount = VertexBuffer->GetNumVertices();
        TSet<FVector> UniqueVertices;
        for (int32 Index = 0; Index < VertexCount; Index++)
        {
            const FVector vertex = static_cast<FVector>(VertexBuffer->VertexPosition(Index));
            UniqueVertices.Add(vertex);
        }
        CVT::Vertices = UniqueVertices.Array();
        UE_LOG(LogTemp, Display, TEXT("Vertices loading completed"))
    }
}

void CVT::SetVertices(TArray<FVector> new_Vertices)
{
    Vertices = new_Vertices;
}

// ���γ��� �� �缳��
void CVT::RefreshRegion()
{
    CVT::Region.Empty();
    CVT::Region.AddUninitialized(CVT::Vertices.Num());

    ParallelFor(CVT::Vertices.Num(), [&](int32 i)
        {
            const FVector& Vertex = CVT::Vertices[i];
            uint32 ClosestSiteIndex = 0;
            float ClosestDistance = FLT_MAX;

            for (int32 j = 0; j < CVT::Sites.Num(); ++j)
            {
                const FVector& Site = CVT::Vertices[CVT::Sites[j]];
                float Distance = FVector::DistSquared(Vertex, Site);

                if (Distance < ClosestDistance)
                {
                    ClosestDistance = Distance;
                    ClosestSiteIndex = j;
                }
            }
            CVT::Region[i] = ClosestSiteIndex;
        });
}

// ���γ��� ���� �����߽��� ���
void CVT::CalculateCentroids()
{
    // ���� setup
    CVT::BaryCenters.Empty();
    CVT::BaryCenters.AddUninitialized(CVT::Sites.Num());

    TArray<FVector> RegionSum;
    TArray<uint32> RegionCount;
    RegionSum.Init(FVector(0, 0, 0), CVT::Sites.Num());
    RegionCount.Init(0, CVT::Sites.Num());

    FCriticalSection Mutex;

    // �� ���ؽ� ���� ó��
    ParallelFor(CVT::Vertices.Num(), [&](int32 i)
        {
            int32 RegionIndex = CVT::Region[i];
            FVector Vertex = CVT::Vertices[i];

            // ���ؽ��� ��ż� ����ȭ
            {
                FScopeLock Lock(&Mutex);
                RegionSum[RegionIndex] += Vertex;
                RegionCount[RegionIndex]++;
            }
        });

    // �����߽� ��ǥ�� ���
    for (int32 i = 0; i < CVT::Sites.Num(); i++)
    {
        if (RegionCount[i] > 0)
        {
            CVT::BaryCenters[i] = RegionSum[i] / RegionCount[i];
        }
        else
        {
            CVT::BaryCenters[i] = FVector(0, 0, 0);  // �� ���� ��� (�ʿ�� �ٸ� ó��)
        }
    }
}
// �����߽��� �������� ���ο� �õ� ����Ʈ ����
TArray<uint32> CVT::GenerateNewSite()
{
    TArray<uint32> NewSites;
    NewSites.AddUninitialized(CVT::Sites.Num());

    ParallelFor(CVT::BaryCenters.Num(), [&](int32 i)
        {
            uint32 ClosestSiteIndex = 0;
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
            NewSites[i] = ClosestSiteIndex;
        });

    return NewSites;
}

// �� �õ� ����Ʈ�� ������ �������� �˻�
bool CVT::isEqualSites(TArray<uint32>& Sites1, TArray<uint32>& Sites2)
{
    for (int32 i = 0; i < Sites1.Num(); ++i)
    {
        if (Sites1[i] != Sites2[i])
            return false;
    }
    return true;
}

void CVT::SetVoronoiSites(TArray<uint32> VoronoiSites)
{
    CVT::Sites = VoronoiSites;
    CVT::RefreshRegion();
}