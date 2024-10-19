#include "DistanceCalculate.h"

std::shared_mutex PredMutex;

TMap<int32, DistOutEntry> DistanceCalculate::Calculate(WeightedGraph& graph, const TArray<int32>& Sources, const int& k)
{
    int32 NumSources = Sources.Num();
    TArray<int32> Vertices = graph.Vertices();
    int32 NumVertices = Vertices.Num();

    TMap<int32, TUniquePtr<std::atomic<double>>> GlobalDist;
    TMap<int32, TUniquePtr<std::atomic<int32>>> VisitedBy;
    
    TArray<DistOutEntry> DistOut;
    DistOut.Init({ std::numeric_limits<double>::infinity(), -1 }, NumVertices);

    for (int32 i = 0; i < NumVertices; ++i)
    {
        GlobalDist.Add(i, MakeUnique<std::atomic<double>>(std::numeric_limits<double>::infinity()));
        VisitedBy.Add(i, MakeUnique<std::atomic<int32>>(-1));
    }

    ParallelFor(NumSources, [&](int32 i)
    {
        using PII = TPair<double, int32>;
        std::priority_queue<PII, std::vector<PII>, std::greater<PII>> Q;

        int32 Src = Vertices.Find(Sources[i]);
        GlobalDist[Src]->store(0.0);
        DistOut[Src] = { 0.0, Sources[i] };
        Q.emplace(0.0, Src);

        while (!Q.empty())
        {
            PII Current = Q.top();
            Q.pop();

            auto [curDist, u] = Current;

            if (VisitedBy[u]->load() != -1 && GlobalDist[u]->load() <= curDist) continue;

            for (const Link& link : graph.getLinks(u))
            {
                int32 v = link.VertexIndex;

                double NewDist = recalculateDistance(graph, u, v, Vertices, GlobalDist[u]->load(), VisitedBy, k);

                if (NewDist < GlobalDist[v]->load())
                {
                    {
                        std::unique_lock<std::shared_mutex> lock(PredMutex);
                        GlobalDist[v]->store(NewDist);
                        VisitedBy[v]->store(u);
                    }
                    Q.emplace(NewDist, v);
                    DistOut[v] = { NewDist, Sources[i] };
                }
            }
        }
    });

    TMap<int32, DistOutEntry> Result;
    for (int32 i = 0; i < NumVertices; ++i)
        Result.Add(Vertices[i], DistOut[i]);

    return Result;
}

double DistanceCalculate::recalculateDistance(WeightedGraph& graph, const int32& u, const int32& v, const TArray<int32>& Vertices, const double& Dist, const TMap<int32, TUniquePtr<std::atomic<int32>>>& Pred, const int& k)
{
    double correctedDist = 0.0;
    TArray<FVector> totalVector;
    int32 current = v;

    const Link* edge = graph.getLink(Vertices[u], Vertices[v]);
    correctedDist += edge->linkVector.Size() + edge->weight;

    {
        std::shared_lock<std::shared_mutex> lock(PredMutex);

        for (int32 i = 0; i < k; ++i)
        {
            int32 pred_i = (current == v ? u : getPredecessor(current, Pred));

            if (pred_i == -1 || !Pred.Contains(pred_i) || Pred[pred_i]->load() == -1) break;

            edge = graph.getLink(Vertices[pred_i], Vertices[current]);
            if (!edge) break;

            FVector edgeVector = edge->linkVector;
            totalVector.Add(edgeVector);
            current = pred_i;
        }
    }

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

int32 DistanceCalculate::getPredecessor(const int32& vertex, const TMap<int32, TUniquePtr<std::atomic<int32>>>& Pred)
{
    auto it = Pred.Find(vertex);
    return (it != nullptr ? (*it)->load() : -1);
}
