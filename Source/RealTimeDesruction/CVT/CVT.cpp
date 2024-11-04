// Fill out your copyright notice in the Description page of Project Settings.

#include "CVT.h"

CVT::CVT()
{
}

CVT::~CVT()
{
}

// 로이드 알고리즘 시행
void CVT::Lloyd_Algo()
{
    // 알고리즘 시행 전 시드포인트 TARRAY 준비
    TArray<FVector3f> OldSites = CVT::Sites;

    // 새 시드포인트가 이전 시트포인트와 일치할때까지 반복
    // 보로노이 셀 재설정 -> 각 셀의 무게중심 구하기 -> 무게중심 기준 새로운 시드포인트 생성
    do
    {
        CVT::CalculateCentroids();
        OldSites = CVT::Sites;
        CVT::Sites = CVT::GenerateNewSite();
        CVT::RefreshRegion();
        UE_LOG(LogTemp, Log, TEXT("Lloyd Algorithm is successfully executed."))
    } while (isEqualSites(CVT::Sites, OldSites));
}

// 스태틱 메쉬 컴포넌트에서 버텍스 정보 가져오는 함수
void CVT::GetVertexDataFromStaticMeshComponent(const UStaticMeshComponent* StaticMeshComponent)
{
    FPositionVertexBuffer* VertexBuffer = &StaticMeshComponent->GetStaticMesh()->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;
    // 버텍스 버퍼가 유효하면 순회하며 Vertices에 버텍스 좌표 추가
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
        UE_LOG(LogTemp, Display, TEXT("Vertices loading completed"))
    }
}

// 보로노이 셀 재설정
void CVT::RefreshRegion()
{
    CVT::Region.Empty();
    CVT::Region.AddUninitialized(CVT::Vertices.Num());

    ParallelFor(CVT::Vertices.Num(), [&](int32 i)
        {
            const FVector3f& Vertex = CVT::Vertices[i];
            int32 ClosestSiteIndex = -1;
            float ClosestDistance = FLT_MAX;

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
            CVT::Region[i] = ClosestSiteIndex;
        });
}

// 보로노이 셀의 무게중심을 계산
void CVT::CalculateCentroids()
{
    // 변수 setup
    CVT::BaryCenters.Empty();
    CVT::BaryCenters.AddUninitialized(CVT::Sites.Num());

    TArray<FVector3f> RegionSum;
    TArray<int32> RegionCount;
    RegionSum.Init(FVector3f(0, 0, 0), CVT::Sites.Num());
    RegionCount.Init(0, CVT::Sites.Num());

    FCriticalSection Mutex;

    // 각 버텍스 병렬 처리
    ParallelFor(CVT::Vertices.Num(), [&](int32 i)
        {
            int32 RegionIndex = CVT::Region[i];
            FVector3f Vertex = CVT::Vertices[i];

            // 뮤텍스를 잠궈서 동기화
            {
                FScopeLock Lock(&Mutex);
                RegionSum[RegionIndex] += Vertex;
                RegionCount[RegionIndex]++;
            }
        });

    // 무게중심 좌표를 계산
    for (int32 i = 0; i < CVT::Sites.Num(); i++)
    {
        if (RegionCount[i] > 0)
        {
            CVT::BaryCenters[i] = RegionSum[i] / RegionCount[i];
        }
        else
        {
            CVT::BaryCenters[i] = FVector3f(0, 0, 0);  // 빈 셀의 경우 (필요시 다른 처리)
        }
    }
}
// 무게중심을 기준으로 새로운 시드 포인트 생성
TArray<FVector3f> CVT::GenerateNewSite()
{
    TArray<FVector3f> NewSites;
    NewSites.AddUninitialized(CVT::Sites.Num());

    ParallelFor(CVT::BaryCenters.Num(), [&](int32 i)
        {
            int32 ClosestSiteIndex = -1;
            float ClosestDistance = FLT_MAX;

            for (int32 j = 0; j < CVT::Vertices.Num(); ++j)
            {
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
        });

    return NewSites;
}

// 새 시드 포인트가 이전과 동일한지 검사
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