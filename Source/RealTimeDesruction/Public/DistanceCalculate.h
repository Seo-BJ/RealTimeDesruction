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
	uint32 Source;
};

class REALTIMEDESRUCTION_API DistanceCalculate
{
public:
	DistanceCalculate() {};

	TMap<uint32, DistOutEntry> Calculate(WeightedGraph& graph, const TArray<uint32>& Sources, const int& k);

	~DistanceCalculate() = default;

private:
	double recalculateDistance(WeightedGraph& graph, const uint32& u, const uint32& v, const double& Dist, const TMap<uint32, TUniquePtr<std::atomic<uint32>>>& Pred, const int& k);
	uint32 getPredecessor(const uint32& vertex, const TMap<uint32, TUniquePtr<std::atomic<uint32>>>& Pred);
};