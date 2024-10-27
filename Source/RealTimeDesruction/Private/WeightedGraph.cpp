// Fill out your copyright notice in the Description page of Project Settings.

#include "WeightedGraph.h"

WeightedGraph::~WeightedGraph()
{
	graph.Empty();
}

bool WeightedGraph::addVertex(const uint32& index)
{
	if (!graph.Contains(index))
	{
		graph.Emplace(index, TArray<Link>());
		return true;
	}

	return false;
}

bool WeightedGraph::deleteVertex(const uint32& index)
{
	if (graph.Remove(index) > 0)
	{
		for (auto& pair : graph)
		{
			deleteLink(pair.Key, index);
		}
		return true;
	}

	return false;
}

bool WeightedGraph::addLink(const uint32& FromIndex, const uint32& ToIndex, const FVector& linkVector, const double& LinkWeight)
{
	addVertex(FromIndex);
	addVertex(ToIndex);


	if (getLink(FromIndex, ToIndex) == nullptr)
	{
		graph[FromIndex].Emplace(Link(ToIndex, LinkWeight, linkVector));
		if (!hasDirection && getLink(ToIndex, FromIndex) == nullptr)
			graph[ToIndex].Emplace(Link(FromIndex, LinkWeight, -linkVector));

		return true;
	}

	return false;
}

bool WeightedGraph::deleteLink(const uint32& FromIndex, const uint32& ToIndex)
{
	auto fromlink = getLink(FromIndex, ToIndex);
	auto tolink = getLink(ToIndex, FromIndex);

	if (fromlink != nullptr)
	{
		graph[FromIndex].RemoveSingle(*fromlink);
		if (!hasDirection && tolink != nullptr)
			graph[ToIndex].RemoveSingle(*tolink);

		return true;
	}

	return false;
}

Link* WeightedGraph::getLink(const uint32& FromIndex, const uint32& ToIndex)
{
	if (!graph.Contains(FromIndex)) return nullptr;

	for (Link& link : graph[FromIndex])
		if (link.VertexIndex == ToIndex)
			return &link;
	return nullptr;
}

TArray<Link> WeightedGraph::getLinks(const uint32& index)
{
	if (!graph.Contains(index)) return TArray<Link>();

	return graph[index];
}

bool WeightedGraph::updateLink(const uint32& FromIndex, const uint32& ToIndex, const double& LinkWeight)
{
	auto fromlink = getLink(FromIndex, ToIndex);
	auto tolink = getLink(ToIndex, FromIndex);

	if (fromlink != nullptr)
	{
		fromlink->weight = LinkWeight;
		if (!hasDirection && tolink != nullptr)
			tolink->weight = LinkWeight;

		return true;
	}

	return false;
}

const uint32 WeightedGraph::size()
{
	return graph.Num();
}

const TArray<uint32> WeightedGraph::Vertices() const
{
	TArray<uint32> vertices;
	graph.GetKeys(vertices);
	return vertices;
}

const bool WeightedGraph::isDirected() const
{
	return hasDirection;
}