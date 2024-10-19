#pragma once

#include <queue>
#include <limits>
#include <functional>
#include <vector>
#include <atomic>
#include <shared_mutex>
#include "CoreMinimal.h"
#include "WeightedGraph.h"
#include "Async/ParallelFor.h"

struct DistOutEntry
{
	double Weight;
	int32 Source;
};

class REALTIMEDESRUCTION_API DistanceCalculate
{
public:
	DistanceCalculate() {};

	TMap<int32, DistOutEntry> Calculate(WeightedGraph& graph, const TArray<int32>& Sources, const int& k);

	~DistanceCalculate() = default;

private:
	double recalculateDistance(WeightedGraph& graph, const int32& u, const int32& v, const TArray<int32>& Vertices, const double& Dist, const TMap<int32, TUniquePtr<std::atomic<int32>>>& Pred, const int& k);
	int32 getPredecessor(const int32& vertex, const TMap<int32, TUniquePtr<std::atomic<int32>>>& Pred);
};