// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

struct Link
{
	int32 VertexIndex;
	double weight;
	FVector linkVector;

	bool operator==(const Link& other) const
	{
		return VertexIndex == other.VertexIndex &&
			FMath::IsNearlyEqual(weight, other.weight) &&
			linkVector.Equals(other.linkVector);
	}
};

class REALTIMEDESRUCTION_API WeightedGraph
{
public:
	WeightedGraph(bool hasDirection) : hasDirection(hasDirection) {};
	~WeightedGraph();

	bool addVertex(const int32& index);

	bool addLink(const int32& FromIndex, const int32& ToIndex, const FVector& linkVector, const double& LinkWeight = 0.0);

	bool deleteLink(const int32& FromIndex, const int32& ToIndex);

	bool deleteVertex(const int32& index);

	Link* getLink(const int32& FromIndex, const int32& ToIndex);

	TArray<Link> getLinks(const int32& index);

	bool updateLink(const int32& FromIndex, const int32& ToIndex, const double& LinkWeight);

	const int32 size();

	const TArray<int32> Vertices() const;

	const bool isDirected() const;

private:
	TMap<int32, TArray<Link>> graph;
	bool hasDirection;
};