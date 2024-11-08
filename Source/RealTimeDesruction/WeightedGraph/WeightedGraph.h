// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

struct Link
{
	uint32 VertexIndex;
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

	bool addVertex(const uint32& index);

	bool addLink(const uint32& FromIndex, const uint32& ToIndex, const FVector& linkVector, const double& LinkWeight = 0.0);

	bool deleteLink(const uint32& FromIndex, const uint32& ToIndex);

	bool deleteVertex(const uint32& index);

	Link* getLink(const uint32& FromIndex, const uint32& ToIndex);

	TArray<Link> getLinks(const uint32& index);

	bool updateLink(const uint32& FromIndex, const uint32& ToIndex, const double& LinkWeight);

	const uint32 size();

	const TArray<uint32> Vertices() const;

	const bool isDirected() const;

private:
	TMap<uint32, TArray<Link>> graph;
	bool hasDirection;
};