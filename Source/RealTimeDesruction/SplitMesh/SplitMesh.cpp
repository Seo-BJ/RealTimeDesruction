
#include "SplitMesh.h"

SplitMesh::SplitMesh(const UStaticMesh* Mesh, const UFEMCalculateComponent* FEMComponent, const TMap<uint32, DistOutEntry>* Distance) : Mesh(Mesh), FEMComponent(FEMComponent), Distance(Distance)
{
	const FStaticMeshLODResources& LODResources = Mesh->GetRenderData()->LODResources[0];
	SplitMesh::PositionVertexBuffer = &LODResources.VertexBuffers.PositionVertexBuffer;
	SplitMesh::IndexBuffer = &LODResources.IndexBuffer;
	NumVertices = FEMComponent->TetMeshVertices.Num();
	this->Tets = &(FEMComponent->Tets);
}

// 거리 기반 사면체 분리 위치 계산
FVector3f SplitMesh::CalculateSplitPoint(const int32& p1, const int32& p2)
{
	FVector3f Point1 = (FVector3f)FEMComponent->TetMeshVertices[p1];
	FVector3f Point2 = (FVector3f)FEMComponent->TetMeshVertices[p2];
	double Dist = FVector3f::Dist(Point1, Point2);
	double D1 = FVector3f::Dist(Point1, (FVector3f)FEMComponent->TetMeshVertices[Distance->FindRef(p1).Source]);
	double D2 = FVector3f::Dist(Point2, (FVector3f)FEMComponent->TetMeshVertices[Distance->FindRef(p1).Source]);
	double Weight = (((D1 + D2 + Dist) / 2) - D2) / Dist;
	auto Result = Point1 + ((Point2 - Point1) * (1 - Weight));
	//UE_LOG(LogTemp, Log, TEXT("Point1: (%f, %f, %f), Point2: (%f, %f, %f), Dist: %f, D1: %f, D2: %f, Weight: %f, Split Point: (%f, %f, %f)"),
	//	Point1.X, Point1.Y, Point1.Z,
	//	Point2.X, Point2.Y, Point2.Z,
	//	Dist,
	//	D1,
	//	D2,
	//	Weight,
	//	Result.X, Result.Y, Result.Z);
	return Result;
}

// 버텍스가 어느 Seed에 속하는지에 따른 사면체 분리
TMap<uint32, TArray<FIntVector4>> SplitMesh::SplitTetra(const FIntVector4& tetra)
{
	TMap<uint32, TArray<FIntVector4>> Result;
	TMap<uint32, TArray<uint32>> Sources;
	TMap<uint32, FVector> Pos;

	for (int i = 0; i < 4; ++i)
	{
		Sources.FindOrAdd(Distance->FindRef(tetra[i]).Source).Emplace((uint32)tetra[i]);
	}

	if (Sources.Num() == 1)
	{
		Result.Emplace(Sources.begin().Key(), TArray({ tetra }));
	}
	else if (Sources.Num() == 2)
	{
		auto Source1 = Sources.begin();
		auto& Source2 = ++Sources.begin();

		if (Source1.Value().Num() == 2 && Source2.Value().Num() == 2)
		{
			FIntVector4 t = FIntVector4(Source1.Value()[0], Source1.Value()[1], Source2.Value()[0], Source2.Value()[1]);
			FVector3f M02 = CalculateSplitPoint(t[0], t[2]);
			FVector3f M03 = CalculateSplitPoint(t[0], t[3]);
			FVector3f M12 = CalculateSplitPoint(t[1], t[2]);
			FVector3f M13 = CalculateSplitPoint(t[1], t[3]);

			uint32 NewIndexM02 = NumVertices++;
			uint32 NewIndexM03 = NumVertices++;
			uint32 NewIndexM12 = NumVertices++;
			uint32 NewIndexM13 = NumVertices++;

			VerticesToAdd.Emplace(M02);
			VerticesToAdd.Emplace(M03);
			VerticesToAdd.Emplace(M12);
			VerticesToAdd.Emplace(M13);

			Result.FindOrAdd(Source1.Key()).Emplace(FIntVector4(t[0], t[1], NewIndexM02, NewIndexM03));
			Result.FindOrAdd(Source1.Key()).Emplace(FIntVector4(t[1], NewIndexM02, NewIndexM03, NewIndexM13));
			Result.FindOrAdd(Source1.Key()).Emplace(FIntVector4(t[1], NewIndexM02, NewIndexM13, NewIndexM12));
			Result.FindOrAdd(Source2.Key()).Emplace(FIntVector4(NewIndexM12, NewIndexM02, NewIndexM13, t[2]));
			Result.FindOrAdd(Source2.Key()).Emplace(FIntVector4(NewIndexM03, NewIndexM02, NewIndexM13, t[2]));
			Result.FindOrAdd(Source2.Key()).Emplace(FIntVector4(t[2], t[3], NewIndexM03, NewIndexM13));
		}
		else
		{
			FIntVector4 t;
			uint32 key1, key2;

			if (Source1.Value().Num() == 1)
			{
				t = FIntVector4(Source1.Value()[0], Source2.Value()[0], Source2.Value()[1], Source2.Value()[2]);
				key1 = Source1.Key();
				key2 = Source2.Key();
			}
			else
			{
				t = FIntVector4(Source2.Value()[0], Source1.Value()[0], Source1.Value()[1], Source1.Value()[2]);
				key1 = Source2.Key();
				key2 = Source1.Key();
			}

			FVector3f M01 = CalculateSplitPoint(t[0], t[1]);
			FVector3f M02 = CalculateSplitPoint(t[0], t[2]);
			FVector3f M03 = CalculateSplitPoint(t[0], t[3]);

			uint32 NewIndexM01 = NumVertices++;
			uint32 NewIndexM02 = NumVertices++;
			uint32 NewIndexM03 = NumVertices++;

			VerticesToAdd.Emplace(M01);
			VerticesToAdd.Emplace(M02);
			VerticesToAdd.Emplace(M03);

			Result.FindOrAdd(key1).Emplace(FIntVector4(t[0], NewIndexM01, NewIndexM02, NewIndexM03));
			Result.FindOrAdd(key2).Emplace(FIntVector4(NewIndexM01, t[1], t[2], t[3]));
			Result.FindOrAdd(key2).Emplace(FIntVector4(NewIndexM02, NewIndexM01, t[2], t[3]));
			Result.FindOrAdd(key2).Emplace(FIntVector4(NewIndexM03, NewIndexM01, NewIndexM02, t[3]));
		}

	}
	else if (Sources.Num() == 3)
	{
		auto Source1 = Sources.begin();
		auto& Source2 = ++Sources.begin();
		auto& Source3 = ++(++Sources.begin());
		FIntVector4 t;
		uint32 key1, key2, key3;

		if (Source1.Value().Num() == Source2.Value().Num())
		{
			t = FIntVector4(Source1.Value()[0], Source2.Value()[0], Source3.Value()[0], Source3.Value()[1]);
			key1 = Source1.Key();
			key2 = Source2.Key();
			key3 = Source3.Key();
		}
		else if (Source1.Value().Num() == Source3.Value().Num())
		{
			t = FIntVector4(Source1.Value()[0], Source3.Value()[0], Source2.Value()[0], Source2.Value()[1]);
			key1 = Source1.Key();
			key2 = Source3.Key();
			key3 = Source2.Key();
		}
		else
		{
			t = FIntVector4(Source2.Value()[0], Source3.Value()[0], Source1.Value()[0], Source1.Value()[1]);
			key1 = Source2.Key();
			key2 = Source3.Key();
			key3 = Source1.Key();
		}

		FVector3f M01 = CalculateSplitPoint(t[0], t[1]);
		FVector3f M02 = CalculateSplitPoint(t[0], t[2]);
		FVector3f M03 = CalculateSplitPoint(t[0], t[3]);
		FVector3f M12 = CalculateSplitPoint(t[1], t[2]);
		FVector3f M13 = CalculateSplitPoint(t[1], t[3]);
		
		uint32 NewIndexM01 = NumVertices++;
		uint32 NewIndexM02 = NumVertices++;
		uint32 NewIndexM03 = NumVertices++;
		uint32 NewIndexM12 = NumVertices++;
		uint32 NewIndexM13 = NumVertices++;

		VerticesToAdd.Emplace(M01);
		VerticesToAdd.Emplace(M02);
		VerticesToAdd.Emplace(M03);
		VerticesToAdd.Emplace(M12);
		VerticesToAdd.Emplace(M13);

		Result.FindOrAdd(key1).Emplace(FIntVector4(t[0], NewIndexM01, NewIndexM02, NewIndexM03));
		Result.FindOrAdd(key1).Emplace(FIntVector4(NewIndexM02, NewIndexM03, NewIndexM01, NewIndexM13));
		Result.FindOrAdd(key2).Emplace(FIntVector4(t[1], NewIndexM12, NewIndexM13, NewIndexM01));
		Result.FindOrAdd(key2).Emplace(FIntVector4(NewIndexM12, NewIndexM13, NewIndexM01, NewIndexM02));
		Result.FindOrAdd(key3).Emplace(FIntVector4(NewIndexM12, t[2], NewIndexM13, NewIndexM02));
		Result.FindOrAdd(key3).Emplace(FIntVector4(t[2], t[3], NewIndexM13, NewIndexM02));
		Result.FindOrAdd(key3).Emplace(FIntVector4(NewIndexM13, t[3], NewIndexM03, NewIndexM02));
	}
	else
	{
		auto Source1 = Sources.begin();
		auto& Source2 = ++Sources.begin();
		auto& Source3 = ++(++Sources.begin());
		auto& Source4 = ++(++(++Sources.begin()));

		FVector3f M01 = CalculateSplitPoint(tetra[0], tetra[1]);
		FVector3f M02 = CalculateSplitPoint(tetra[0], tetra[2]);
		FVector3f M03 = CalculateSplitPoint(tetra[0], tetra[3]);
		FVector3f M12 = CalculateSplitPoint(tetra[1], tetra[2]);
		FVector3f M13 = CalculateSplitPoint(tetra[1], tetra[3]);
		FVector3f M23 = CalculateSplitPoint(tetra[2], tetra[3]);

		uint32 NewIndexM01 = NumVertices++;
		uint32 NewIndexM02 = NumVertices++;
		uint32 NewIndexM03 = NumVertices++;
		uint32 NewIndexM12 = NumVertices++;
		uint32 NewIndexM13 = NumVertices++;
		uint32 NewIndexM23 = NumVertices++;

		VerticesToAdd.Emplace(M01);
		VerticesToAdd.Emplace(M02);
		VerticesToAdd.Emplace(M03);
		VerticesToAdd.Emplace(M12);
		VerticesToAdd.Emplace(M13);
		VerticesToAdd.Emplace(M23);

		Result.FindOrAdd(Source1.Key()).Emplace(FIntVector4(tetra[0], NewIndexM01, NewIndexM02, NewIndexM03));
		Result.FindOrAdd(Source1.Key()).Emplace(FIntVector4(NewIndexM02, NewIndexM03, NewIndexM01, NewIndexM13));
		Result.FindOrAdd(Source2.Key()).Emplace(FIntVector4(tetra[1], NewIndexM12, NewIndexM13, NewIndexM01));
		Result.FindOrAdd(Source2.Key()).Emplace(FIntVector4(NewIndexM12, NewIndexM13, NewIndexM02, NewIndexM01));
		Result.FindOrAdd(Source3.Key()).Emplace(FIntVector4(tetra[2], NewIndexM23, NewIndexM12, NewIndexM02));
		Result.FindOrAdd(Source3.Key()).Emplace(FIntVector4(NewIndexM23, NewIndexM13, NewIndexM02, NewIndexM12));
		Result.FindOrAdd(Source4.Key()).Emplace(FIntVector4(tetra[3], NewIndexM13, NewIndexM23, NewIndexM03));
		Result.FindOrAdd(Source4.Key()).Emplace(FIntVector4(NewIndexM13, NewIndexM23, NewIndexM03, NewIndexM02));
	}

	TArray<uint32> s;
	Sources.GenerateKeyArray(s);
	Seed.Append(s);

	return Result;
}

void SplitMesh::GenerateCombinations(const TArray<int32>& InputArray, TArray<int32>& CurrentCombination, int32 StartIndex, int32 CombinationSize, TArray<TArray<int32>>& Result) {
	if (CurrentCombination.Num() == CombinationSize) {
		Result.Add(CurrentCombination);
		return;
	}

	for (int32 i = StartIndex; i < InputArray.Num(); i++) {
		CurrentCombination.Add(InputArray[i]);
		GenerateCombinations(InputArray, CurrentCombination, i + 1, CombinationSize, Result);
		CurrentCombination.Pop();
	}
}

// 사면체에서 메쉬를 분리하고 새 메쉬 반환
TMap<uint32, UProceduralMeshComponent*> SplitMesh::Split()
{
	TMap<uint32, UProceduralMeshComponent*> Meshes;
	TMap<uint32, TArray<FIntVector4>> TetMeshes;
	TMap<uint32, TArray<FVector>> Vertices;
	TMap<uint32, TArray<int32>> Triangles;
	TMap<uint32, TArray<TSet<int32>>> TriangleDupCheck;

	TMap<uint32, uint32> link;
	for (int32 i = 0; i < (int32)PositionVertexBuffer->GetNumVertices(); ++i)
	{
		FVector3f pos = PositionVertexBuffer->VertexPosition(i);
		for (int32 j = 0; j < FEMComponent->TetMeshVertices.Num(); ++j)
		{
			if ((FEMComponent->TetMeshVertices[j] - (FVector)pos).IsNearlyZero())
			{
				link.Emplace(i, j);
				break;
			}
		}
	}

	// 기존 버텍스 추가
	for (int32 i = 0; i < IndexBuffer->GetNumIndices(); i += 3)
	{
		TArray<int32> Index;

		for (int j = 0; j < 3; ++j)
		{
			int32 idx = (int32)IndexBuffer->GetIndex(i + j);
			FVector vtx = FVector(PositionVertexBuffer->VertexPosition(idx));

			Index.Emplace(link.FindRef(idx));
			Vertices.FindOrAdd(Distance->FindRef(link.FindRef(idx)).Source).Emplace(vtx);
		}

		if (Distance->FindRef(Index[0]).Source == Distance->FindRef(Index[1]).Source &&
			Distance->FindRef(Index[1]).Source == Distance->FindRef(Index[2]).Source)
		{
			Triangles.FindOrAdd(Distance->FindRef(Index[0]).Source).Append(Index);
			//UE_LOG(LogTemp, Log, TEXT("%d: [%d, %d, %d]"), Distance->FindRef(Index[0]).Source, Index[0], Index[1], Index[2]);
		}
	}

	// 사면체 분리 후 시드별 저장
	for (int i = 0; i < Tets->Num(); ++i)
	{
		auto result = SplitTetra((*Tets)[i]);
		for (auto& r : result)
		{
			TetMeshes.FindOrAdd(r.Key).Append(r.Value);
		}
	}

	auto SeedArray = Seed.Array();
	SeedArray.Sort();

	// 새로운 메쉬 선언
	for (int i = 0; i < SeedArray.Num(); ++i)
	{
		Meshes.FindOrAdd(SeedArray[i]) = NewObject<UProceduralMeshComponent>();
	}


	for (int32 i = 0; i < SeedArray.Num(); ++i)
	{
		for (int32 j = 0; j < Triangles.FindRef(SeedArray[i]).Num(); j += 3)
		{
			TriangleDupCheck.FindOrAdd(SeedArray[i]).Emplace(TSet({ Triangles.FindRef(SeedArray[i])[j], Triangles.FindRef(SeedArray[i])[j + 1], Triangles.FindRef(SeedArray[i])[j + 2] }));
		}
		Vertices.FindOrAdd(SeedArray[i]);
		TriangleDupCheck.FindOrAdd(SeedArray[i]);
		Triangles.FindOrAdd(SeedArray[i]);
	}

	TArray<int32> arr({ 0, 1, 2, 3 });
	TArray<TArray<int32>> AllCombinations;
	TArray<int32> CurrentCombination;
	GenerateCombinations(arr, CurrentCombination, 0, 3, AllCombinations);

	FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
		{
			ParallelFor(SeedArray.Num(), [&](int32 Index)
				{
					uint32 MeshKey = SeedArray[Index];
					TArray<FIntVector4>& TetValue = TetMeshes[MeshKey];

					TArray<FVector>& VertexArray = Vertices.FindOrAdd(MeshKey);
					TArray<int32>& TriangleArray = Triangles.FindOrAdd(MeshKey);
					TArray<TSet<int32>>& TriangleDup = TriangleDupCheck.FindOrAdd(MeshKey);

					for (auto& t : TetValue)
					{
						FVector3d Center = FVector3d(0.0, 0.0, 0.0);
						TArray<int32> PosIndices;
						TArray<FVector3d> Pos;

						for (int i = 0; i < 4; ++i)
						{
							FVector VertexPos;
							if (t[i] < FEMComponent->TetMeshVertices.Num())
							{
								VertexPos = FVector(FEMComponent->TetMeshVertices[t[i]]);
							}
							else
							{
								VertexPos = FVector(VerticesToAdd[t[i] - FEMComponent->TetMeshVertices.Num()]);
							}

							int32 VertexIndex;
							if (!VertexArray.Contains(VertexPos))
							{
								VertexArray.Emplace(VertexPos);
							}
							VertexIndex = VertexArray.IndexOfByKey(VertexPos);

							PosIndices.Add(VertexIndex);
							Pos.Emplace(VertexPos);
							Center += FVector3d(VertexPos);
						}

						Center /= 4;

						for (auto& Comb : AllCombinations)
						{
							auto normal = FVector::CrossProduct(Pos[Comb[1]] - Pos[Comb[0]], Pos[Comb[2]] - Pos[Comb[0]]);
							auto isFacing = FVector::DotProduct(normal, Center - Pos[Comb[0]]);
						
							if (normal.IsNearlyZero())
							{
								continue;
							}

							TArray<int32> Indices;

							if (isFacing > 0)
							{
								Indices = { PosIndices[Comb[0]], PosIndices[Comb[1]], PosIndices[Comb[2]] };
							}
							else
							{
								Indices = { PosIndices[Comb[2]], PosIndices[Comb[1]], PosIndices[Comb[0]] };
							}

							TSet<int32> keySet(Indices);
							int32 idx = INDEX_NONE;

							for (int32 i = 0; i < TriangleDup.Num(); ++i)
							{
								if (!(TriangleDup[i].Difference(keySet).Num()))
								{
									idx = i;
									break;
								}
							}

							if (idx == INDEX_NONE)
							{
								TriangleArray.Append(Indices);
								TriangleDup.Emplace(keySet);
							}
							else
							{
								for (int i = 2; i >= 0; --i)
								{
									TriangleArray.RemoveAt(3 * idx + i);
								}
								TriangleDup.RemoveAt(idx);
							}
						}
					}
				}
			);
		}
	);

	FTaskGraphInterface::Get().WaitUntilTaskCompletes(Task);

	//FString VerticesLog;
	//for (const FVector3f& Vertex : VerticesToAdd)
	//{
	//	VerticesLog += FString::Printf(TEXT("(%f, %f, %f), "), Vertex.X, Vertex.Y, Vertex.Z);
	//}

	//if (VerticesLog.Len() > 2)
	//{
	//	VerticesLog.LeftChopInline(2);
	//}

	//UE_LOG(LogTemp, Log, TEXT("VerticesToAdd: [%s]"), *VerticesLog);


	if (SeedArray.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Seed count is zero."));
	}

	FString ArrayString;

	// 배열 요소를 문자열로 변환하여 연결
	for (int32 Element : SeedArray)
	{
		ArrayString += FString::Printf(TEXT("%d "), Element);
	}

	UE_LOG(LogTemp, Log, TEXT("Seed: %s"), *ArrayString);
	// 새 메쉬 추가
	for (int idx = 0; idx < SeedArray.Num(); ++idx)
	{
		uint32 Index = SeedArray[idx];
		if (Vertices.Contains(Index) && Triangles.Contains(Index) && Meshes.Contains(Index))
		{
			Meshes[Index]->CreateMeshSection(
				0,
				Vertices.FindRef(Index),
				Triangles.FindRef(Index),
				TArray<FVector>(),
				TArray<FVector2D>(),
				TArray<FColor>(),
				TArray<FProcMeshTangent>(),
				true
			);
			Meshes[Index]->AddCollisionConvexMesh(Vertices.FindRef(Index));
			UE_LOG(LogTemp, Log, TEXT("Vertex %d, Triangle %d"), Vertices.FindRef(Index).Num(), Triangles.FindRef(Index).Num());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Vertex Init Error."));
		}
	}

	return Meshes;
}