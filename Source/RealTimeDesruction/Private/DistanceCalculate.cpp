#include "DistanceCalculate.h"

FCriticalSection QueueMutex;
FCriticalSection VisitedMutex;

TMap<int32, double> DistanceCalculate::Calculate(WeightedGraph& graph, const TArray<int32>& Sources, const int& k)
{
    int32 NumSources = Sources.Num();
    int32 NumVertices = graph.Vertices().Num();

    TArray<TMap<int32, double>> DistPerSource;
    DistPerSource.SetNum(NumSources);

    TArray<TMap<int32, bool>> VisitedPerSource;
    VisitedPerSource.SetNum(NumSources);

    TArray<TMap<int32, int32>> PredPerSource;
    PredPerSource.SetNum(NumSources);

    TArray<DistOutEntry> DistOut;
    DistOut.Init({ std::numeric_limits<double>::infinity(), -1 }, NumVertices);
    
    for (int32 i = 0; i < NumSources; ++i)
    {
        for (int32 j = 0; j < NumVertices; ++j)
        {
            DistPerSource[i].Add(j, std::numeric_limits<double>::infinity());
            VisitedPerSource[i].Add(j, false);
            PredPerSource[i].Add(j, -1);
        }
    }

    ParallelFor(NumSources, [&](int32 i)
    {
        using PII = TPair<double, int32>;
        std::priority_queue<PII, std::vector<PII>, std::greater<PII>> Q;

        int32 Src = Sources[i];
        DistPerSource[i][Src] = 0.0;
        Q.emplace(0.0, Src);

        while (!Q.empty())
        {
            PII Current;

            {
                FScopeLock Lock(&QueueMutex);
                Current = Q.top();
                Q.pop();
            }

            auto [curDist, u] = Current;

            {
                FScopeLock Lock(&VisitedMutex);
                if (VisitedPerSource[i][u]) continue;

                VisitedPerSource[i][u] = true;
            }

            for (const Link& link : graph.getLinks(u))
            {
                int32 v = link.VertexIndex;
                double Weight = link.linkVector.Size() + link.weight;

                double NewDist = recalculateDistance(graph, u, v, DistPerSource[i], PredPerSource[i], k);

                if (NewDist < DistPerSource[i][v]) 
                {
                    DistPerSource[i][v] = NewDist;
                    PredPerSource[i][v] = u;

                    {
                        FScopeLock Lock(&QueueMutex);
                        DistOut[v] = DistOutEntry({ NewDist, i });
                        Q.emplace(NewDist, v);
                    }
                }
            }
        }
    });

    TMap<int32, double> Result;
    for (int32 i = 0; i < NumVertices; ++i)
    {
        Result.Add(i, DistOut[i].Weight);
    }
    return Result;
}

double DistanceCalculate::recalculateDistance(WeightedGraph& graph, const int32& u, int32& v, TMap<int32, double>& Dist, TMap<int32, int32>& Pred, const int& k)
{
    int32 OriginalPredecessor = Pred.FindChecked(u);
    Pred[v] = u;

    double correctedDist = 0.0;
    TArray<FVector> totalVector;
    int32 current = v;

    for (int32 i = 0; i < k; ++i)
    {
        int32 pred_i = getPredecessor(current, Pred);
        if (pred_i == -1) break;

        if (Pred.Contains(pred_i) && Pred[pred_i] != -1)
        {
            const Link* edge = graph.getLink(pred_i, current);
            if (edge)
            {
                FVector edgeVector = edge->linkVector;
                totalVector.Add(edgeVector);
                correctedDist += edgeVector.Size() + edge->weight;
                current = pred_i;
            }
            else break;
        }
        else break;
    }

    if (totalVector.Num() > 1)
    {
        for (int32 i = 1; i < totalVector.Num(); ++i)
        {
            FVector direction = totalVector[i].GetSafeNormal();
            FVector prevDirection = totalVector[i - 1].GetSafeNormal();
            double angle = 1.0 - FVector::DotProduct(direction, prevDirection);
            correctedDist += angle;
        }
    }

    Pred[v] = OriginalPredecessor;

    return Dist[u] + correctedDist;
}

int32 DistanceCalculate::getPredecessor(const int32& vertex, const TMap<int32, int32>& Pred)
{
    return Pred.FindRef(vertex, -1);
}
