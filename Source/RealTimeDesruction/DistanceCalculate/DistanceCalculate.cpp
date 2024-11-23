#include "DistanceCalculate.h"

std::shared_mutex PredMutex;

constexpr uint32 MaxUInt32 = std::numeric_limits<uint32>::max();
constexpr double MaxDouble = std::numeric_limits<double>::infinity();

TMap<uint32, DistOutEntry> DistanceCalculate::Calculate(WeightedGraph& graph, const TArray<uint32>& Sources, const int& k)
{
    uint32 NumSources = Sources.Num();
    TArray<uint32> Vertices = graph.Vertices();
    uint32 NumVertices = Vertices.Num();

    TMap<uint32, TUniquePtr<std::atomic<double>>> GlobalDist; // Source에서 해당 Vertex까지의 최단 거리
    TMap<uint32, TUniquePtr<std::atomic<uint32>>> VisitedBy; // 최단 거리의 경로 추적을 위한 이전 경로 Vertex
    
    TArray<DistOutEntry> DistOut; // 최단 거리, 가장 가까운 Source를 저장. 결과 리턴을 위함

    //초기화
    DistOut.Init({ MaxDouble, MaxUInt32 }, Vertices.Max());

    for (uint32 i = 0; i <= (uint32)Vertices.Max(); ++i)
    {
        GlobalDist.Add(i, MakeUnique<std::atomic<double>>(MaxDouble));
        VisitedBy.Add(i, MakeUnique<std::atomic<uint32>>(i));
    }

    // Source의 최단 경로는 Source
    for (uint32 i = 0; i < NumSources; i++)
    {
        uint32 Src = Sources[i];
        GlobalDist[Src]->store(0.0);
        DistOut[Src] = { 0.0, Src };
        VisitedBy[Src]->store(Src);
    }

    // 병렬 처리
    //ParallelFor(NumSources, [&](uint32 i)
    for (uint32 i = 0; i < NumSources; i++)
    {
        using PII = TPair<double, uint32>;
        std::priority_queue<PII, std::vector<PII>, std::greater<PII>> Q;

        // 우선 순위 큐에 최초 경로 삽입
        Q.emplace(0.0, Sources[i]);
        while (!Q.empty())
        {
            PII Current = Q.top();
            Q.pop();

            auto [curDist, u] = Current;

            for (const Link& link : graph.getLinks(u))
            {
                uint32 v = link.VertexIndex;

                // 이미 유망하지 않다면 탈출
                if (VisitedBy[v]->load() != v && GlobalDist[v]->load() <= curDist)
                    continue;

                // 다음 경로의 거리 계산
                double NewDist = recalculateDistance(graph, u, v, GlobalDist[u]->load(), VisitedBy, k);

                // 거리가 짧다면 업데이트
                if (NewDist < GlobalDist[v]->load())
                {
                    {
                        // 스레드 락
                        std::unique_lock<std::shared_mutex> lock(PredMutex);
                        GlobalDist[v]->store(NewDist);
                        VisitedBy[v]->store(u);
                    }
                    Q.emplace(NewDist, v);
                    DistOut[v] = { NewDist, Sources[i] };
                }
            }
        }
        //});
    }

    // 결과
    TMap<uint32, DistOutEntry> Result;
    for (uint32 i = 0; i < (uint32)Vertices.Max(); ++i)
        if (DistOut[i].Weight != MaxDouble)
            Result.Add(Vertices[i], DistOut[i]);

    return Result;
}

// 방향 벡터를 고려한 거리 계산
double DistanceCalculate::recalculateDistance(WeightedGraph& graph, const uint32& u, const uint32& v, const double& Dist, const TMap<uint32, TUniquePtr<std::atomic<uint32>>>& Pred, const int& k)
{
    double correctedDist = 0.0;
    TArray<FVector> totalVector;
    uint32 current = v;

    // 초기 거리
    const Link* edge = graph.getLink(u, v);
    correctedDist += edge->linkVector.Size() + edge->weight;
    
    {
        // 스레드 락
        std::shared_lock<std::shared_mutex> lock(PredMutex);

        for (int32 i = 0; i < k; ++i)
        {
            // 이전 경로 탐색
            uint32 pred_i = (current == v ? u : getPredecessor(current, Pred));

            if (pred_i == current || pred_i == MaxUInt32) break;

            edge = graph.getLink(pred_i, current);
            if (!edge) break;

            // 경로 벡터 저장
            FVector edgeVector = edge->linkVector;
            totalVector.Add(edgeVector);
            current = pred_i;
        }
    }

    // 내적하여 경로의 방향 계산
    if (totalVector.Num() > 1)
    {
        FVector prevDirection = totalVector[0].GetSafeNormal();

        for (int32 i = 1; i < totalVector.Num(); ++i)
        {
            const FVector direction = totalVector[i].GetSafeNormal();
            double angle = 1.0 - FVector::DotProduct(direction, prevDirection);
            correctedDist += angle;
            prevDirection = direction;
        }
    }

    return Dist + correctedDist;
}

// 이전 경로 Vertex 탐색
uint32 DistanceCalculate::getPredecessor(const uint32& vertex, const TMap<uint32, TUniquePtr<std::atomic<uint32>>>& Pred)
{
    auto it = Pred.Find(vertex);
    return (it != nullptr ? (*it)->load() : MaxUInt32);
}
