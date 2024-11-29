
#include "SplitMesh.h"

SplitMesh::SplitMesh(const UStaticMesh* Mesh, const UTetMeshGenerateComponent* TetMesh, const TMap<uint32, DistOutEntry>* Distance) : Mesh(Mesh), TetMesh(TetMesh), Distance(Distance)
{
	const FStaticMeshLODResources& LODResources = Mesh->GetRenderData()->LODResources[0];
	SplitMesh::PositionVertexBuffer = &LODResources.VertexBuffers.PositionVertexBuffer;
	SplitMesh::IndexBuffer = &LODResources.IndexBuffer;
	NumVertices = TetMesh->TetMeshVertices.Num();
	this->Tets = &(TetMesh->Tets);
}

// 그래프 거리 기반 사면체 분리 위치 계산
FVector3f SplitMesh::CalculateSplitPoint(const int32& p1, const int32& p2)
{
	FVector3f Point1 = (FVector3f)TetMesh->TetMeshVertices[p1];
	FVector3f Point2 = (FVector3f)TetMesh->TetMeshVertices[p2];
	double Dist = FVector3f::Dist(Point1, Point2);
	double D1 = FVector3f::Dist(Point1, (FVector3f)TetMesh->TetMeshVertices[Distance->FindRef(p1).Source]);
	double D2 = FVector3f::Dist(Point2, (FVector3f)TetMesh->TetMeshVertices[Distance->FindRef(p2).Source]);
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
		//Result.Emplace(Sources.begin().Key(), TArray({ tetra }));
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
			Result.FindOrAdd(Source2.Key()).Emplace(FIntVector4(NewIndexM12, NewIndexM02, NewIndexM03, t[2]));
			Result.FindOrAdd(Source2.Key()).Emplace(FIntVector4(NewIndexM12, NewIndexM03, NewIndexM13, t[2]));
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
		Result.FindOrAdd(Source2.Key()).Emplace(FIntVector4(NewIndexM12, NewIndexM13, NewIndexM02, NewIndexM23));
		Result.FindOrAdd(Source3.Key()).Emplace(FIntVector4(tetra[2], NewIndexM23, NewIndexM12, NewIndexM02));
		Result.FindOrAdd(Source4.Key()).Emplace(FIntVector4(tetra[3], NewIndexM13, NewIndexM23, NewIndexM03));
		Result.FindOrAdd(Source4.Key()).Emplace(FIntVector4(NewIndexM13, NewIndexM23, NewIndexM03, NewIndexM02));
	}

	TArray<uint32> s;
	Sources.GenerateKeyArray(s);
	Seed.Append(s);

	return Result;
}

// 사면체에서 메쉬를 분리하고 새 메쉬 반환
TMap<uint32, UProceduralMeshComponent*> SplitMesh::Split()
{
	TMap<uint32, UProceduralMeshComponent*> Meshes;
	TMap<uint32, TArray<FIntVector4>> TetMeshes;
	TMap<uint32, TArray<FVector>> Vertices;
	TMap<uint32, TArray<int32>> Triangles;

	TMap<uint32, uint32> link;
	for (int32 i = 0; i < (int32)PositionVertexBuffer->GetNumVertices(); ++i)
	{
		FVector3f pos = PositionVertexBuffer->VertexPosition(i);
		for (int32 j = 0; j < TetMesh->TetMeshVertices.Num(); ++j)
		{
			if ((TetMesh->TetMeshVertices[j] - (FVector)pos).IsNearlyZero())
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
			UE_LOG(LogTemp, Log, TEXT("%d: [%d, %d, %d]"), Distance->FindRef(Index[0]).Source, Index[0], Index[1], Index[2]);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("%d Vertices."), Vertices.Num());

	// 사면체 분리 후 시드별 저장
	for (int i = 0; i < Tets->Num(); ++i)
	{
		auto result = SplitTetra((*Tets)[i]);
		for (auto& r : result)
		{
			TetMeshes.FindOrAdd(r.Key).Append(r.Value);
		}
	}
	
	// 새로운 메쉬 선언
	for (int i = 0; i < Seed.Num(); ++i)
	{
		Meshes.FindOrAdd(Seed.Array()[i]) = NewObject<UProceduralMeshComponent>();
	}

	for (auto& tet : TetMeshes)
	{
		uint32 MeshKey = tet.Key;
		auto& VertexArray = Vertices.FindOrAdd(MeshKey);
		auto& TriangleArray = Triangles.FindOrAdd(MeshKey);
		TArray<TSet<int32>> TriangleDupCheck;
		for (int32 i = 0; i < TriangleArray.Num(); i += 3)
		{
			TriangleDupCheck.Emplace(TSet({TriangleArray[i], TriangleArray[i + 1], TriangleArray[i + 2]}));
		}


		for (auto& t : tet.Value)
		{
			FVector3d Center = FVector3d(0.0, 0.0, 0.0);
			TArray<int32> PosIndices;
			TArray<FVector3d> Pos;

			// 외적을 이용한 삼각형 인덱스 결정
			// 삼각형의 외적이 사면체의 중심을 향한다면 안쪽을 보고 있는 것이므로 반대 방향으로 회전
			for (int i = 0; i < 4; ++i)
			{
				FVector VertexPos;
				if (t[i] < TetMesh->TetMeshVertices.Num())
				{
					VertexPos = FVector(TetMesh->TetMeshVertices[t[i]]);
				}
				else
				{
					VertexPos = FVector(VerticesToAdd[t[i] - TetMesh->TetMeshVertices.Num()]);
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

			for (int i = 0; i < 4; ++i)
			{
				auto normal = FVector::CrossProduct(Pos[(i + 1) % 4] - Pos[i], Pos[(i + 2) % 4] - Pos[i]);

				auto isFacing = FVector::DotProduct(normal, Center - Pos[i]);
				
				if (normal.IsNearlyZero())
				{
					continue;
				}

				TArray<int32> Indices;

				if (isFacing < 0)
				{
					Indices = { PosIndices[i], PosIndices[(i + 2) % 4], PosIndices[(i + 1) % 4] };
				}
				else
				{
					Indices = { PosIndices[i], PosIndices[(i + 1) % 4], PosIndices[(i + 2) % 4] };
				}

				TSet<int32> keySet = TSet(Indices);
				int32 idx = INDEX_NONE;


				for (int j = 0; j < TriangleDupCheck.Num(); ++j)
				{
					if (!TriangleDupCheck[j].Difference(keySet).Num())
					{
						idx = j;
						break;
					}
				}

				if (idx == INDEX_NONE)
				{
					TriangleArray.Append(Indices);
					TriangleDupCheck.Emplace(keySet);
				}
				else
				{
					for (int j = 2; j >= 0; --j)
					{
						TriangleArray.RemoveAt(3 * idx + j);
					}
					TriangleDupCheck.RemoveAt(idx);
				}
			}
		}
	}
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
	if (Seed.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Seed count is zero."));
	}

	FString ArrayString;

	// 배열 요소를 문자열로 변환하여 연결
	for (int32 Element : Seed)
	{
		ArrayString += FString::Printf(TEXT("%d "), Element);
	}

	UE_LOG(LogTemp, Log, TEXT("Seed: %s"), *ArrayString);
	// 새 메쉬 추가
	for (int idx = 0; idx < Seed.Num(); ++idx)
	{
		uint32 Index = Seed.Array()[idx];
		if (Vertices.Contains(Index) && Triangles.Contains(Index))
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
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Vertex Init Error."));
		}
	}

	return Meshes;
}