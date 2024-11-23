// Fill out your copyright notice in the Description page of Project Settings.


#include "VoroTestComponent.h"

// For Voronoi Cells
static TMap<int32, FColor> ColorMap = {
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

	// ...
	TetMeshComponent = GetOwner()->FindComponentByClass<UTetMeshGenerateComponent>();

	if (TetMeshComponent != nullptr)
	{
		GetOwner()->FindComponentByClass<UStaticMeshComponent>()->SetVisibility(bMeshVisibility);

		TetMeshComponent->Graph;

		Sites = getRandomVoronoiSites(TetMeshComponent->TetMeshVertices.Num());

		if (bUseCVT)
		{
			CVT CVT_inst;

			CVT_inst.SetVertices(TetMeshComponent->TetMeshVertices);
			CVT_inst.SetVoronoiSites(Sites);
			CVT_inst.Lloyd_Algo();
			Sites = CVT_inst.Sites;
			Region = CVT_inst.Region;
		}

		Region.Empty();
		Region.AddUninitialized(TetMeshComponent->TetMeshVertices.Num());

		DistanceCalculate DistCalc;
		TMap<uint32, DistOutEntry> DistanceMap;
		DistanceMap = DistCalc.Calculate(TetMeshComponent->Graph, Sites, 3);

		for (const TPair<uint32,DistOutEntry> &dist : DistanceMap)
			Region[dist.Key] = Sites.Find(dist.Value.Source);

		VisualizeVertices();
		DestroyActor(TetMeshComponent, &DistanceMap);
	}
}

// Called every frame
void UVoroTestComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

TArray<uint32> UVoroTestComponent::getRandomVoronoiSites(const int32 VeticesSize)
{
	TArray<uint32> VoronoiSites;
	TSet<uint32> SelectedIndices;

	SiteNum = FMath::Min(SiteNum, VeticesSize);

	while (SelectedIndices.Num() < SiteNum)
	{
		// ���� �ε��� ����
		uint32 RandomIndex = FMath::RandRange(0, VeticesSize - 1);
		SelectedIndices.Emplace(RandomIndex);
	}
	VoronoiSites = SelectedIndices.Array();
	VoronoiSites.Sort();

	return VoronoiSites;
}

void UVoroTestComponent::VisualizeVertices()
{
	for (int32 i = 0; i < TetMeshComponent->TetMeshVertices.Num(); i++)
	{
		FVector WorldPosition = GetOwner()->GetActorTransform().TransformPosition(static_cast<FVector>(TetMeshComponent->TetMeshVertices[i]));
		int32 RegionOfVertex = Region[i];

		//DrawDebugString(GetWorld(), WorldPosition + FVector(0, 0, 20), FString::Printf(TEXT("%d"), i));

		if (i == RegionOfVertex)
			DrawDebugSphere(GetWorld(), WorldPosition, 5.0f, 1.0f, FColor::White, true, -1.0f, 0);
		else
			DrawDebugPoint(GetWorld(), WorldPosition, 15.0f, ColorMap[RegionOfVertex % ColorMap.Num()], true, -1.0f, 0);
	}
}

void UVoroTestComponent::DestroyActor(const UTetMeshGenerateComponent* TetComponent, const TMap<uint32, DistOutEntry>* Dist)
{
	UStaticMeshComponent* MeshComponent = GetOwner()->FindComponentByClass<UStaticMeshComponent>();
	TMap<uint32, UProceduralMeshComponent*> Meshes;
	
	const UStaticMesh* Mesh = MeshComponent->GetStaticMesh();

	auto MeshSplit = SplitMesh(Mesh, TetComponent, Dist);
	Meshes = MeshSplit.Split();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("World is not valid! Cannot spawn actors."));
		return;
	}

	TArray<uint32> key;
	Meshes.GenerateKeyArray(key);
	auto& PositionBuffer = Mesh->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;

	if (Meshes.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Mesh to generate is not exist."));
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("Mesh Count: %d"), Meshes.Num());
	
	for (int32 i = 0; i < Meshes.Num(); ++i)
	{
		FVector SpawnLocation = GetOwner()->GetActorTransform().TransformPosition((FVector)PositionBuffer.VertexPosition(key[i]));
		FRotator SpawnRotation = GetOwner()->GetActorRotation();
		
		FActorSpawnParameters SpawnParams;
		//SpawnParams.Owner = GetOwner(); // 이 컴포넌트를 가진 액터를 소환한 액터의 소유자로 설정
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AStaticMeshActor* NewActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), SpawnLocation, SpawnRotation, SpawnParams);
		UE_LOG(LogTemp, Error, TEXT("New Actor Created."));
		if (NewActor)
		{
			UStaticMesh* StaticMesh = NewObject<UStaticMesh>(NewActor);
			if (!StaticMesh)
			{
				UE_LOG(LogTemp, Error, TEXT("StaticMesh is null."));
				continue;
			}

			FMeshDescription MeshDescription;
			FStaticMeshAttributes Attributes(MeshDescription);
			Attributes.Register();

			for (int32 SectionIndex = 0; SectionIndex < Meshes[key[i]]->GetNumSections(); ++SectionIndex)
			{

				auto* MeshSection = Meshes[key[i]]->GetProcMeshSection(SectionIndex);
				if (MeshSection)
				{
					TArray<FProcMeshVertex> Vertices = MeshSection->ProcVertexBuffer;
					auto& Triangles = MeshSection->ProcIndexBuffer;

					TMap<int32, FVertexID> VertexMap;
					for (int32 VertexIndex = 0; VertexIndex < Vertices.Num(); ++VertexIndex)
					{
						FVertexID VertexID = MeshDescription.CreateVertex();
						Attributes.GetVertexPositions()[VertexID] = (FVector3f)Vertices[VertexIndex].Position;
						VertexMap.Add(VertexIndex, VertexID);
					}

					FPolygonGroupID PolygonGroupID = MeshDescription.CreatePolygonGroup();
					for (int32 TriangleIndex = 0; TriangleIndex < Triangles.Num(); TriangleIndex += 3)
					{
						TArray<FVertexInstanceID> VertexInstanceIDs;
						for (int32 j = 0; j < 3; ++j)
						{
							FVertexInstanceID VertexInstanceID = MeshDescription.CreateVertexInstance(VertexMap[Triangles[TriangleIndex + j]]);
							VertexInstanceIDs.Add(VertexInstanceID);
						}

						MeshDescription.CreatePolygon(PolygonGroupID, VertexInstanceIDs);
					}
				}
			}

			StaticMesh->BuildFromMeshDescriptions({ &MeshDescription });
			UStaticMeshComponent* NewMeshComponent = NewActor->GetStaticMeshComponent();
			if (NewMeshComponent)
			{
				NewMeshComponent->SetStaticMesh(StaticMesh);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to get StaticMeshComponent from spawned actor."));
				continue;
			}
			
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to spawn actor at location: %s"), *SpawnLocation.ToString());
		}
	}
}