
#include "SplitMesh.h"

SplitMesh::SplitMesh(const UStaticMesh* Mesh, const TMap<uint32, DistOutEntry>* Distance) : Mesh(Mesh), Distance(Distance)
{
	const FStaticMeshLODResources& LODResources = Mesh->GetRenderData()->LODResources[0];
	SplitMesh::PositionVertexBuffer = &LODResources.VertexBuffers.PositionVertexBuffer;
	SplitMesh::IndexBuffer = &LODResources.IndexBuffer;
	NumVertices = LODResources.GetNumVertices();
}

// 그래프 거리 기반 사면체 분리 위치 계산
FVector3f SplitMesh::CalculateSplitPoint(const int32& p1, const int32& p2)
{
	FVector3f Point1 = PositionVertexBuffer->VertexPosition(p1);
	FVector3f Point2 = PositionVertexBuffer->VertexPosition(p2);
	double Dist = (Point1 - Point2).Size();
	double Weight = (((Distance->FindRef(p1).Weight + Distance->FindRef(p2).Weight + Dist) / 2) - Distance->FindRef(p2).Weight) / Dist;
	return Point1 + ((Point2 - Point1) * (1 - Weight));
}

// 버텍스가 어느 Seed에 속하는지에 따른 사면체 분리
TMap<uint32, FIntVector4> SplitMesh::SplitTetra(const FIntVector4& tetra)
{
	TMap<uint32, FIntVector4> Result;
	TMap<uint32, TArray<uint32>> Sources;

	for (int i = 0; i < 4; ++i)
	{
		Sources[Distance->FindRef(tetra[i]).Source].Emplace(tetra[i]);
	}

	if (Sources.Num() == 1)
	{
		// Result.Emplace(Sources.begin().Key(), tetra);
		// Source가 1이라면 경계가 아님
		// 경계의 사면체만 추가하여 메쉬 최소화
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

			Result.Emplace(Source1.Key(), FIntVector4(t[0], t[1], NewIndexM02, NewIndexM03));
			Result.Emplace(Source1.Key(), FIntVector4(t[1], NewIndexM02, NewIndexM03, NewIndexM13));
			Result.Emplace(Source1.Key(), FIntVector4(t[1], NewIndexM02, NewIndexM13, NewIndexM12));
			Result.Emplace(Source2.Key(), FIntVector4(NewIndexM12, NewIndexM02, NewIndexM03, t[2]));
			Result.Emplace(Source2.Key(), FIntVector4(NewIndexM12, NewIndexM03, NewIndexM13, t[2]));
			Result.Emplace(Source2.Key(), FIntVector4(t[2], t[3], NewIndexM03, NewIndexM13));
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

			Result.Emplace(key1, FIntVector4(t[0], NewIndexM01, NewIndexM02, NewIndexM03));
			Result.Emplace(key2, FIntVector4(NewIndexM01, t[1], t[2], t[3]));
			Result.Emplace(key2, FIntVector4(NewIndexM02, NewIndexM01, t[2], t[3]));
			Result.Emplace(key2, FIntVector4(NewIndexM03, NewIndexM01, NewIndexM02, t[3]));
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

		Result.Emplace(key1, FIntVector4(t[0], NewIndexM01, NewIndexM02, NewIndexM03));
		Result.Emplace(key1, FIntVector4(NewIndexM02, NewIndexM03, NewIndexM01, NewIndexM13));
		Result.Emplace(key2, FIntVector4(t[1], NewIndexM12, NewIndexM13, NewIndexM01));
		Result.Emplace(key2, FIntVector4(NewIndexM12, NewIndexM13, NewIndexM01, NewIndexM02));
		Result.Emplace(key3, FIntVector4(NewIndexM12, t[2], NewIndexM13, NewIndexM02));
		Result.Emplace(key3, FIntVector4(t[2], t[3], NewIndexM13, NewIndexM02));
		Result.Emplace(key3, FIntVector4(NewIndexM13, t[3], NewIndexM03, NewIndexM02));
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

		Result.Emplace(Source1.Key(), FIntVector4(tetra[0], NewIndexM01, NewIndexM02, NewIndexM03));
		Result.Emplace(Source1.Key(), FIntVector4(NewIndexM02, NewIndexM03, NewIndexM01, NewIndexM13));
		Result.Emplace(Source2.Key(), FIntVector4(tetra[1], NewIndexM12, NewIndexM13, NewIndexM01));
		Result.Emplace(Source2.Key(), FIntVector4(NewIndexM12, NewIndexM13, NewIndexM02, NewIndexM23));
		Result.Emplace(Source3.Key(), FIntVector4(tetra[2], NewIndexM23, NewIndexM12, NewIndexM02));
		Result.Emplace(Source4.Key(), FIntVector4(tetra[3], NewIndexM13, NewIndexM23, NewIndexM03));
		Result.Emplace(Source4.Key(), FIntVector4(NewIndexM13, NewIndexM23, NewIndexM03, NewIndexM02));
	}

	Sources.GetKeys(Seed);
	return Result;
}

// 사면체에서 메쉬를 분리하고 새 메쉬 반환
TArray<UProceduralMeshComponent*> SplitMesh::Split(const TArray<FIntVector4>& Tets)
{
	TArray<UProceduralMeshComponent*> Meshes;
	TMap<uint32, TArray<FIntVector4>> TetMesh;
	std::mutex m;
	TMap<uint32, uint32> Idx;
	TMap<uint32, TArray<FVector>> Vertices;
	TMap<uint32, TArray<int32>> Triangles;

	// 새로운 메쉬 선언
	for (int i = 0; i < Seed.Num(); ++i)
	{
		Meshes.Emplace(NewObject<UProceduralMeshComponent>());
		Idx.Emplace(Seed.Array()[i], i);
	}
	
	// 기존 버텍스 추가
	for (int32 i = 0; i < IndexBuffer->GetNumIndices(); i += 3)
	{
		TArray<int32> Index;

		for (int j = 0; j < 3; ++j)
		{
			int32 idx = (int32)IndexBuffer->GetIndex(i + j);
			FVector vtx = FVector(PositionVertexBuffer->VertexPosition(idx));

			Index.Emplace(idx);
			Vertices[Distance->FindRef(idx).Source].Emplace(vtx);
		}

		if (Distance->FindRef(Index[0]).Source == Distance->FindRef(Index[1]).Source == Distance->FindRef(Index[2]).Source)
		{
			Triangles.Emplace(Distance->FindRef(Index[0]).Source, Index);
		}
	}

	// 사면체 분리 후 시드별 저장
	ParallelFor(Tets.Num(), [&](uint32 i)
		{
			auto result = SplitTetra(Tets[i]);
			for (auto& r : result)
			{
				std::unique_lock<std::mutex> lock(m);
				TetMesh.FindRef(r.Key).Emplace(r.Value);
			}
		});
	
	for (auto& tet : TetMesh)
	{
		uint32 MeshKey = tet.Key;
		auto& VertexArray = Vertices[MeshKey];
		auto& TriangleArray = Triangles[MeshKey];

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
				if ((uint32)t[i] < PositionVertexBuffer->GetNumVertices())
				{
					VertexPos = FVector(PositionVertexBuffer->VertexPosition(t[i]));
				}
				else
				{
					VertexPos = FVector(VerticesToAdd[t[i] - PositionVertexBuffer->GetNumVertices()]);
				}

				int32 VertexIndex;
				if (VertexArray.Contains(VertexPos))
				{
					VertexIndex = VertexArray.Find(VertexPos);
				}
				else
				{
					VertexIndex = VertexArray.Emplace(VertexPos);
				}

				PosIndices.Add(VertexIndex);
				Pos.Emplace(VertexPos);
				Center += FVector3d(VertexPos);
			}

			Center /= 4;

			for (int i = 0; i < 4; ++i)
			{
				auto normal = FVector::CrossProduct(Pos[(i + 1) % 4] - Pos[i], Pos[(i + 2) % 4] - Pos[i]);

				auto isFacing = FVector::DotProduct(normal, Center - Pos[i]);

				if (isFacing > 0)
				{
					TriangleArray.Add(PosIndices[i]);
					TriangleArray.Add(PosIndices[(i + 2) % 4]);
					TriangleArray.Add(PosIndices[(i + 1) % 4]);
				}
				else
				{
					TriangleArray.Add(PosIndices[i]);
					TriangleArray.Add(PosIndices[(i + 1) % 4]);
					TriangleArray.Add(PosIndices[(i + 2) % 4]);
				}
			}
		}
	}
	
	// 새 메쉬 추가
	for (auto& MeshKey : Idx)
	{
		uint32 Index = MeshKey.Value;
		if (Vertices.Contains(MeshKey.Key) && Triangles.Contains(MeshKey.Key))
		{
			Meshes[Index]->CreateMeshSection(
				0,
				Vertices[MeshKey.Key],
				Triangles[MeshKey.Key],
				TArray<FVector>(),
				TArray<FVector2D>(),
				TArray<FColor>(),
				TArray<FProcMeshTangent>(),
				true 
			);
		}
	}

	return Meshes;
}