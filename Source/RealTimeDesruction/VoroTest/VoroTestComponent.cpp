// Fill out your copyright notice in the Description page of Project Settings.


#include "VoroTestComponent.h"

// For Voronoi Cells
TMap<int32, FColor> ColorMap = {
	{0, FColor::Red},
	{1, FColor::Green},
	{2, FColor::Blue},
	{3, FColor::Yellow},
	{4, FColor::Cyan},
	{5, FColor::Magenta},
	{6, FColor::Turquoise},
	{7, FColor::Silver}
};

// Sets default values for this component's properties
UVoroTestComponent::UVoroTestComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	// ...
}


// Called when the game starts
void UVoroTestComponent::BeginPlay()
{
	Super::BeginPlay();
	FEMComponent = GetOwner()->FindComponentByClass<UFEMCalculateComponent>();
}

void UVoroTestComponent::DestructMesh(const float Energy)
{
	UpdateGraphWeight(Energy, FEMComponent->CurrentImpactPoint);
	if (bUseRandomSeed)
		Seeds = getVoronoiSeedByRandom();
	else
		Seeds = getVoronoiSeedByImpactPoint(FEMComponent->CurrentImpactPoint);

	if (bUseCVT)
	{
		CVT CVT_inst;

		CVT_inst.SetVertices(FEMComponent->TetMeshVertices);
		CVT_inst.SetVoronoiSites(Seeds);
		CVT_inst.Lloyd_Algo();
		Seeds = CVT_inst.Sites;
		Region = CVT_inst.Region;
	}

	Region.Empty();
	Region.AddUninitialized(FEMComponent->TetMeshVertices.Num());

	DistanceCalculate DistCalc;
	TMap<uint32, DistOutEntry> DistanceMap;
	DistanceMap = DistCalc.Calculate(FEMComponent->Graph, Seeds, 3);

	for (const TPair<uint32, DistOutEntry>& dist : DistanceMap)
		Region[dist.Key] = Seeds.Find(dist.Value.Source);

	//VisualizeVertices();
	DestroyActor(&DistanceMap);
}

void UVoroTestComponent::UpdateGraphWeight(const float Energy, const TArray<uint32> ImpactPoint)
{
	WeightedGraph* Graph = &(FEMComponent->Graph);
	TSet<uint32> VisitedVertex;
	TSet<uint32> NextVertexLayer;
	TMap<uint32, float> EnergyMap;
	TMap<uint32, uint32> EnergyMap_Contributed;

	const float FACTOR_DIST_DAMPING = 0.01f;
	
	for (uint32 i = 0; i < Graph->size(); ++i)
	{
		EnergyMap.Add(i, 0);
		EnergyMap_Contributed.Add(i, 0);
	}

	NextVertexLayer.Append(ImpactPoint);
	EnergyMap[ImpactPoint[0]] = Energy;
	EnergyMap[ImpactPoint[1]] = Energy;
	EnergyMap[ImpactPoint[2]] = Energy;

	EnergyMap_Contributed[ImpactPoint[0]] = 1;
	EnergyMap_Contributed[ImpactPoint[1]] = 1;
	EnergyMap_Contributed[ImpactPoint[2]] = 1;

	// Calc Vertex Energy
	while(!NextVertexLayer.IsEmpty())
	{
		TSet<uint32> CurVertexLayer = NextVertexLayer;
		VisitedVertex.Append(CurVertexLayer);
		NextVertexLayer.Empty();

		for (const auto vtx : CurVertexLayer)
		{
			TArray<Link> links = Graph->getLinks(vtx);
			for (const auto& link : links)
			{
				// 방문하지 않은 노드라면
				if (!VisitedVertex.Contains(link.VertexIndex))
				{
					EnergyMap[link.VertexIndex] += EnergyMap[vtx] / (1 + link.linkVector.Size() * FACTOR_DIST_DAMPING);
					EnergyMap_Contributed[link.VertexIndex]++;
					NextVertexLayer.Emplace(link.VertexIndex);
				}
			}
		}
		for (const auto vtx : NextVertexLayer)
			EnergyMap[vtx] = EnergyMap[vtx] / EnergyMap_Contributed[vtx];

		// Update Graph Weight
		for (const auto vtx : CurVertexLayer)
		{
			TArray<Link> links = Graph->getLinks(vtx);
			for (const auto& link : links)
			{
				float NewWeight = (EnergyMap[link.VertexIndex] + EnergyMap[vtx]) / 2;
				Graph->updateLink(vtx, link.VertexIndex, NewWeight);
			}
		}
	}
	
	// Visualize Vertex Energy
	/*
	for (int32 i = 0; i < FEMComponent->TetMeshVertices.Num(); i++)
	{
		FVector WorldPosition = GetOwner()->GetActorTransform().TransformPosition(static_cast<FVector>(FEMComponent->TetMeshVertices[i]));

		FColor color = FColor::Black;
		color.G = 255 * (1 - EnergyMap[i] / Energy);
		color.R = 255 * EnergyMap[i] / Energy;

		DrawDebugPoint(GetWorld(), WorldPosition, 15.0f, color, true, -1.0f, 0);
	}
	for (int32 i = 0; i < ImpactPoint.Num(); i++)
	{
		FVector WorldPosition = GetOwner()->GetActorTransform().TransformPosition(static_cast<FVector>(FEMComponent->TetMeshVertices[ImpactPoint[i]]));
		DrawDebugPoint(GetWorld(), WorldPosition, 15.0f, FColor::White, true, -1.0f, 0);
	}*/
}

// Called every frame
void UVoroTestComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

TArray<uint32> UVoroTestComponent::getVoronoiSeedByImpactPoint(const TArray<uint32> ImpactPoint)
{
	WeightedGraph* Graph = &(FEMComponent->Graph);
	TArray<uint32> VoronoiSeeds;
	TSet<uint32> VisitedVertex;
	TSet<uint32> NextVertexLayer;
	
	if (SeedNum <= 3)
		VoronoiSeeds = getRandomElementsFromArray(ImpactPoint, SeedNum);
	else
	{
		// 그래프 순회하며 주변 버텍스 선택
		NextVertexLayer.Append(ImpactPoint);
		while ((uint32)VoronoiSeeds.Num() < SeedNum)
		{
			TSet<uint32> CurVertexLayer = NextVertexLayer;
			VisitedVertex.Append(CurVertexLayer);
			NextVertexLayer.Empty();

			for (const auto vtx : CurVertexLayer)
			{
				TArray<Link> links = Graph->getLinks(vtx);
				for (const auto& link : links)
				{
					if (!VisitedVertex.Contains(link.VertexIndex))
						NextVertexLayer.Emplace(link.VertexIndex);
				}
			}
			if ((uint32)(NextVertexLayer.Num() + VoronoiSeeds.Num()) > SeedNum)
				VoronoiSeeds.Append(getRandomElementsFromArray(NextVertexLayer.Array(), SeedNum - VoronoiSeeds.Num()));
			else
				VoronoiSeeds.Append(NextVertexLayer.Array());
		}
	}

	VoronoiSeeds.Sort();
	return VoronoiSeeds;
}

TArray<uint32> UVoroTestComponent::getVoronoiSeedByRandom()
{
	TArray<uint32> VoronoiSeeds;
	TSet<uint32> SelectedIndices;
	int32 VeticesSize = FEMComponent->TetMeshVertices.Num();

	while ((uint32)SelectedIndices.Num() < SeedNum)
	{
		// ���� �ε��� ����
		uint32 RandomIndex = FMath::RandRange(0, VeticesSize - 1);
		SelectedIndices.Emplace(RandomIndex);
	}
	VoronoiSeeds = SelectedIndices.Array();
	VoronoiSeeds.Sort();

	return VoronoiSeeds;
}

void UVoroTestComponent::VisualizeVertices()
{
	for (int32 i = 0; i < FEMComponent->TetMeshVertices.Num(); i++)
	{
		FVector WorldPosition = GetOwner()->GetActorTransform().TransformPosition(static_cast<FVector>(FEMComponent->TetMeshVertices[i]));
		int32 RegionOfVertex = Region[i];

		//DrawDebugString(GetWorld(), WorldPosition + FVector(0, 0, 20), FString::Printf(TEXT("%d"), i));

		if (i == RegionOfVertex)
			DrawDebugSphere(GetWorld(), WorldPosition, 5.0f, 1.0f, FColor::White, true, -1.0f, 0);
		else
			DrawDebugPoint(GetWorld(), WorldPosition, 15.0f, ColorMap[RegionOfVertex % ColorMap.Num()], true, -1.0f, 0);
	}
}

void UVoroTestComponent::DestroyActor(const TMap<uint32, DistOutEntry>* Dist)
{
	UStaticMeshComponent* MeshComponent = GetOwner()->FindComponentByClass<UStaticMeshComponent>();
	if (!MeshComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("MeshComponent not found on the owner."));
		return;
	}

	TMap<uint32, UProceduralMeshComponent*> Meshes;
	const UStaticMesh* Mesh = MeshComponent->GetStaticMesh();
	if (!Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("StaticMesh is not assigned to MeshComponent."));
		return;
	}

	auto MeshSplit = SplitMesh(Mesh, FEMComponent, Dist);
	Meshes = MeshSplit.Split();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("World is not valid! Cannot spawn actors."));
		return;
	}

	TArray<uint32> key;
	Meshes.GetKeys(key);
	auto& PositionBuffer = Mesh->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;

	if (key.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Mesh to generate is not exist."));
		return;
	}

	TArray<UMaterialInterface*> Materials;
	for (int32 MaterialIndex = 0; MaterialIndex < MeshComponent->GetNumMaterials(); ++MaterialIndex)
	{
		UMaterialInterface* Material = MeshComponent->GetMaterial(MaterialIndex);
		if (Material)
		{
			Materials.Add(Material);
		}
	}
	
	for (int32 i = 0; i < key.Num(); ++i)
	{
		FVector SpawnLocation = GetOwner()->GetActorLocation();
		FRotator SpawnRotation = GetOwner()->GetActorRotation();
		FVector SpawnScale = GetOwner()->GetActorScale();

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ASplitActor* NewActor = World->SpawnActor<ASplitActor>(ASplitActor::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
		if (!NewActor)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to spawn actor at location: %s"), *SpawnLocation.ToString());
			continue;
		}

		NewActor->SetActorScale3D(SpawnScale);
		NewActor->SetProceduralMesh(Meshes.FindRef(key[i]), MeshComponent->GetMaterials());
		NewActor->GenerateCollisionConvexMesh();
	}

	GetOwner()->Destroy(true);
}