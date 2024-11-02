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
        UE_LOG(LogTemp, Log, TEXT("DONE"))
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
        UE_LOG(LogTemp, Display, TEXT("Vertices loading complete"))
    }
}

// 보로노이 셀 재설정
void CVT::RefreshRegion()
{
    // 보로노이 셀 TATTAY 초기화
    CVT::Region.Empty();
    CVT::Region.AddUninitialized(CVT::Vertices.Num());

    // 버텍스 TARRAY 순회하며 각 버텍스가 시드 포인트 TARRY의 몇번째 셀과 제일 가까운지 확인 후 저장
    for (int32 i = 0; i < CVT::Vertices.Num(); ++i)
    {
        // 변수 setup
        const FVector3f& Vertex = CVT::Vertices[i];
        int32 ClosestSiteIndex = -1;
        float ClosestDistance = FLT_MAX;

        // 시드 포인트 TARRY 순회하며 몇번째 셀이 제일 가까운지 확인 (유클리드 거리)
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

        // 버텍스 TARRAY의 i번째 버텍스가 시드 포인트 TARRY의 몇번째 셀과 제일 가까운지 저장
        CVT::Region[i] = ClosestSiteIndex;
    }
}

// 보로노이 셀의 무게중심을 계산
void CVT::CalculateCentroids()
{
    // 변수 setup
    CVT::BaryCenters.Empty();
    CVT::BaryCenters.AddUninitialized(CVT::Sites.Num());

    TMap<int32, FVector3f> RegionSum;
    TMap<int32, int32> RegionCount;

    // 버텍스 TARRAY 순회하며 시드포인트끼리 좌표값을 더함, 그리고 몇개를 더했는지도 기록
    for (int32 i = 0; i < CVT::Vertices.Num(); ++i)
    {
        int32 RegionIndex = CVT::Region[i];

        // 해당 셀에 대한 계산이 처음인지 아닌지에 따라 다르게 처리
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

    // 더해진 좌표값을 평균을 내서 무게중심 좌표를 계산
    for (int32 i = 0; i < CVT::Sites.Num(); i++)
    {
        FVector3f Centroid = RegionSum[i] / RegionCount[i];
        UE_LOG(LogTemp, Log, TEXT("index : %d  centroid : x=%f, y=%f, z=%f"), i, Centroid.X, Centroid.Y, Centroid.Z);
        CVT::BaryCenters[i] = Centroid;
    }
}

// 무게중심을 기준으로 새로운 시드 포인트 생성
TArray<FVector3f> CVT::GenerateNewSite()
{
    // 새 TARRAY 생성 및 초기화
    TArray<FVector3f> NewSites;
    NewSites.AddUninitialized(CVT::Sites.Num());

    // 무게중심 TARRAY를 순회하며 시드포인트를 생성
    for (int32 i = 0; i < CVT::BaryCenters.Num(); ++i)
    {
        int32 ClosestSiteIndex = -1;
        float ClosestDistance = FLT_MAX;

        // 무게중심에서 제일 가까운 버텍스를 시드포인트로 지정 (유클리드 거리)
        for (int32 j = 0; j < CVT::Vertices.Num(); ++j)
        {
            // 셀에 속하지 않는 버텍스는 무시
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