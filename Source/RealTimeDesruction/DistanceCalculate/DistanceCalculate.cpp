#include "DistanceCalculate.h"

std::shared_mutex PredMutex;

constexpr uint32 MaxUInt32 = std::numeric_limits<uint32>::max();
constexpr double MaxDouble = std::numeric_limits<double>::infinity();

TMap<uint32, DistOutEntry> DistanceCalculate::Calculate(WeightedGraph& graph, const TArray<uint32>& Sources, const int& k)
{
    uint32 NumSources = Sources.Num();
    TArray<uint32> Vertices = graph.Vertices();
    uint32 NumVertices = Vertices.Num();

    TMap<uint32, TUniquePtr<std::atomic<double>>> GlobalDist; // Source���� �ش� Vertex������ �ִ� �Ÿ�
    TMap<uint32, TUniquePtr<std::atomic<uint32>>> VisitedBy; // �ִ� �Ÿ��� ��� ������ ���� ���� ��� Vertex
    
    TArray<DistOutEntry> DistOut; // �ִ� �Ÿ�, ���� ����� Source�� ����. ��� ������ ����

    //�ʱ�ȭ
    DistOut.Init({ MaxDouble, MaxUInt32 }, Vertices.Max());

    for (uint32 i = 0; i <= (uint32)Vertices.Max(); ++i)
    {
        GlobalDist.Add(i, MakeUnique<std::atomic<double>>(MaxDouble));
        VisitedBy.Add(i, MakeUnique<std::atomic<uint32>>(i));
    }

    // Source�� �ִ� ��δ� Source
    for (uint32 i = 0; i < NumSources; i++)
    {
        uint32 Src = Sources[i];
        GlobalDist[Src]->store(0.0);
        DistOut[Src] = { 0.0, Src };
        VisitedBy[Src]->store(Src);
    }

    // ���� ó��
    TArray<TFuture<void>> Futures;

    for (uint32 i = 0; i < NumSources; i++)
    {
        Futures.Add(Async(EAsyncExecution::ThreadPool, [&, i]()
            {
                using PII = TPair<double, uint32>;
                std::priority_queue<PII, std::vector<PII>, std::greater<PII>> Q;

                // �켱 ���� ť�� ���� ��� ����
                Q.emplace(0.0, Sources[i]);
                while (!Q.empty())
                {
                    PII Current = Q.top();
                    Q.pop();

                    auto [curDist, u] = Current;

                    for (const Link& link : graph.getLinks(u))
                    {
                        uint32 v = link.VertexIndex;

                        // �̹� �������� �ʴٸ� Ż��
                        if (VisitedBy[v]->load() != v && GlobalDist[v]->load() <= curDist)
                            continue;

                        // ���� ����� �Ÿ� ���
                        double NewDist = recalculateDistance(graph, u, v, GlobalDist[u]->load(), VisitedBy, k);

                        // �Ÿ��� ª�ٸ� ������Ʈ
                        if (NewDist < GlobalDist[v]->load())
                        {
                            {
                                // ������ ��
                                std::unique_lock<std::shared_mutex> lock(PredMutex);
                                GlobalDist[v]->store(NewDist);
                                VisitedBy[v]->store(u);
                            }
                            Q.emplace(NewDist, v);
                            DistOut[v] = { NewDist, Sources[i] };
                        }
                    }
                }
            }
        ));
    }

    for (auto& Future : Futures)
    {
        Future.Wait();
    }

    // ���
    TMap<uint32, DistOutEntry> Result;
    for (uint32 i = 0; i < (uint32)Vertices.Max(); ++i)
        if (DistOut[i].Weight != MaxDouble)
            Result.Add(Vertices[i], DistOut[i]);

    return Result;
}

// ���� ���͸� ����� �Ÿ� ���
double DistanceCalculate::recalculateDistance(WeightedGraph& graph, const uint32& u, const uint32& v, const double& Dist, const TMap<uint32, TUniquePtr<std::atomic<uint32>>>& Pred, const int& k)
{
    double correctedDist = 0.0;
    TArray<FVector> totalVector;
    uint32 current = v;

    // �ʱ� �Ÿ�
    const Link* edge = graph.getLink(u, v);
    correctedDist += edge->linkVector.Size() + edge->weight;
    
    {
        // ������ ��
        std::shared_lock<std::shared_mutex> lock(PredMutex);

        for (int32 i = 0; i < k; ++i)
        {
            // ���� ��� Ž��
            uint32 pred_i = (current == v ? u : getPredecessor(current, Pred));

            if (pred_i == current || pred_i == MaxUInt32) break;

            edge = graph.getLink(pred_i, current);
            if (!edge) break;

            // ��� ���� ����
            FVector edgeVector = edge->linkVector;
            totalVector.Add(edgeVector);
            current = pred_i;
        }
    }

    // �����Ͽ� ����� ���� ���
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

// ���� ��� Vertex Ž��
uint32 DistanceCalculate::getPredecessor(const uint32& vertex, const TMap<uint32, TUniquePtr<std::atomic<uint32>>>& Pred)
{
    auto it = Pred.Find(vertex);
    return (it != nullptr ? (*it)->load() : MaxUInt32);
}
