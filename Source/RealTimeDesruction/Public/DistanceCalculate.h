#pragma once

#include <queue>
#include <limits>
#include <functional>
#include <vector>
#include "CoreMinimal.h"
#include "WeightedGraph.h"
#include "HAL/CriticalSection.h"
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

	TMap<int32, double> Calculate(WeightedGraph& graph, const TArray<int32>& Sources, const int& k);

	~DistanceCalculate() = default;

private:
	double recalculateDistance(WeightedGraph& graph, const int32& u, int32& v, TMap<int32, double>& Dist, TMap<int32, int32>& Pred, const int& k);
	int32 getPredecessor(const int32& vertex, const TMap<int32, int32>& Pred);
};