// Fill out your copyright notice in the Description page of Project Settings.


#include "FEMCalculateComponent.h"

#include "FTetWildWrapper.h"
#include "Engine/StaticMesh.h"
#include "TetMeshGenerateComponent.h"

#include "FTetWildWrapper.h"

using namespace Eigen;

// Sets default values for this component's properties
UFEMCalculateComponent::UFEMCalculateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

}
// Called when the game starts
void UFEMCalculateComponent::BeginPlay()
{
	Super::BeginPlay();
    InitializeTetMesh();
}

void UFEMCalculateComponent::InitializeTetMesh()
{
    // StaticMesh�� ����
    TArray<FVector> Verts;
    TArray<FIntVector3> Tris;

    // Todo : Get Verts and Tris
    if (UStaticMeshComponent* StaticMeshComponent = GetOwner()->FindComponentByClass<UStaticMeshComponent>())
    {
        if (UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh())
        {
            // Access LOD 0 for simplicity, modify if you need other LODs
            const FStaticMeshLODResources& LODResource = StaticMesh->GetRenderData()->LODResources[0];

            // Get vertices
            const FPositionVertexBuffer& VertexBuffer = LODResource.VertexBuffers.PositionVertexBuffer;
            const int32 VertexCount = VertexBuffer.GetNumVertices();
            Verts.Reserve(VertexCount);
            for (int32 i = 0; i < VertexCount; i++)
            {
                double PositionX = VertexBuffer.VertexPosition(i).X;
                double PositionY = VertexBuffer.VertexPosition(i).Y;
                double PositionZ = VertexBuffer.VertexPosition(i).Z;
                FVector PositionVector = FVector(PositionX, PositionY, PositionZ);
                Verts.Add(PositionVector);
            }

            // Get triangle indices
            const FIndexArrayView& IndexArray = LODResource.IndexBuffer.GetArrayView();
            const int32 IndexCount = IndexArray.Num();
            Tris.Reserve(IndexCount / 3);
            for (int32 i = 0; i < IndexCount; i += 3)
            {
                Tris.Add(FIntVector3(IndexArray[i], IndexArray[i + 1], IndexArray[i + 2]));
            }
        }
    }


    UE::Geometry::FTetWild::FTetMeshParameters Params;
    Params.bCoarsen = bCoarsen;
    Params.bExtractManifoldBoundarySurface = bExtractManifoldBoundarySurface;
    Params.bSkipSimplification = bSkipSimplification;

    Params.EpsRel = EpsRel;
    Params.MaxIts = MaxIterations;
    Params.StopEnergy = StopEnergy;
    Params.IdealEdgeLength = IdealEdgeLength;

    Params.bInvertOutputTets = bInvertOutputTets;

    FProgressCancel Progress;
    UE_LOG(LogTemp, Warning, TEXT("Generating tet mesh via TetWild..."));
    if (UE::Geometry::FTetWild::ComputeTetMesh(Params, Verts, Tris, TetMeshVertices, Tets, &Progress))
    {
        UE_LOG(LogTemp, Warning, TEXT("Succefully Generated tet mesh!"));

        UE_LOG(LogTemp, Display, TEXT("Vertex : %d"), TetMeshVertices.Num());
        UE_LOG(LogTemp, Display, TEXT("Tets : %d"), Tets.Num());

        AActor* OnwerActor = GetOwner();
        UFEMCalculateComponent* FEMComponent = OnwerActor->FindComponentByClass<UFEMCalculateComponent>();
        SetUndeformedPositions();
        KMatrix();

        /*
        for (int32 i = 0; i < TetMeshVertices.Num(); i++)
        {
            const FVector& Vertex = TetMeshVertices[i];
            UE_LOG(LogTemp, Display, TEXT("Vertex %d: X=%f, Y=%f, Z=%f"), i, Vertex.X, Vertex.Y, Vertex.Z);
        }
        // Logging Tets
        UE_LOG(LogTemp, Display, TEXT("Tets:"));
        for (int32 i = 0; i < Tets.Num(); i++)
        {
            const FIntVector4& Tet = Tets[i];
            UE_LOG(LogTemp, Display, TEXT("Tet %d: V0=%d, V1=%d, V2=%d, V3=%d"), i, Tet.X, Tet.Y, Tet.Z, Tet.W);
        }
        */
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to Generate tet mesh!"));
    }

}

float UFEMCalculateComponent::CalculateEnergyAtTatUsingFEM(const FVector& Velocity, const FVector& NextTickVelocity, const float Mass, const FVector& HitPoint)
{
    int32 ExcludedIndex = 0;
    FInt32Vector4 ClosestResult = GetClosestTriangleAndTet(HitPoint, ExcludedIndex);

    // Hit���� �� �м��� �� Tet�� �ε����� �ﰢ�� �� ����
    int32 TargetTetIndex = ClosestResult[0];
    FVector A = TetMeshVertices[ClosestResult[1]];
    FVector B = TetMeshVertices[ClosestResult[2]];
    FVector C = TetMeshVertices[ClosestResult[3]];

    Matrix<float, 9, 1> F = CalculateImpactForceMatrix(Velocity, NextTickVelocity, Mass, HitPoint, { A, B, C });
    Matrix<float, 9, 9> K = SubKMatrix(KElements[TargetTetIndex], { ClosestResult[1], ClosestResult[2], ClosestResult[3] });
    Matrix<float, 4, 4> u = UMatrix(K, F, { ClosestResult[1], ClosestResult[2], ClosestResult[3] }, ExcludedIndex);
    LogMatrix<Matrix<float, 9, 1>>(F);
    LogMatrix<Matrix<float, 9, 9>>(K);
    LogMatrix<Matrix<float, 4, 4>>(u);
    UE_LOG(LogTemp, Warning, TEXT("Excluded Index = %d"), ExcludedIndex);
    float TargetTetVolume = GetTetVolume(Tets[TargetTetIndex]);

    Matrix<float, 4, 4> Dm;
    TArray<int32> VertexIndex;
    VertexIndex.Add(Tets[TargetTetIndex].X);
    VertexIndex.Add(Tets[TargetTetIndex].Y);
    VertexIndex.Add(Tets[TargetTetIndex].Z);
    VertexIndex.Add(Tets[TargetTetIndex].W);

    // 4x4 ��ġ ��� Dm ����
    for (int vtx = 0; vtx < 4; vtx++)
    {
        for (int dim = 0; dim < 3; dim++)
        {
            Dm(dim, vtx) = UndeformedPositions[3 * VertexIndex[vtx] + dim];
        }
    }
    for (int j = 0; j < 4; j++)
    {
        Dm(3, j) = 1;
    }
    return CalculateEnergy(Dm, u, TargetTetVolume);
}



float UFEMCalculateComponent::CalculateEnergy(Matrix<float, 4, 4> DmMatrix, Matrix<float, 4, 4> UMatrix, float TetVolume)
{
    Matrix<float, 4, 4> DsMatrix = DmMatrix + UMatrix;
    Matrix<float, 4, 4> GMatrix = DsMatrix * DmMatrix.inverse();
    Matrix<float, 4, 4> Identity;
    Identity.Identity();
    Matrix<float, 4, 4> StrainTensorMatrix = (GMatrix + GMatrix.transpose()) / 2 - Identity;
    float DoubleDotProduct = 0.f;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            DoubleDotProduct += (StrainTensorMatrix(i, j) * StrainTensorMatrix(i, j));
        }
    }
    float Trace = StrainTensorMatrix(0,0) + StrainTensorMatrix(1, 1) + StrainTensorMatrix(2, 2) + StrainTensorMatrix(3, 3);
    float EnergyDensity = Mu * DoubleDotProduct + 0.5 * Lambda * Trace * Trace;
    float Energy = EnergyDensity * TetVolume;

    return Energy;
}

Matrix<float, 4, 4> UFEMCalculateComponent::UMatrix(Matrix<float, 9, 9> KMatrix, Matrix<float, 9, 1> FMatrix, const FInt32Vector3 TriangleIndex, int32 ExcludedIndex)
{
    Matrix<float, 9, 1> UMatrix9x1;
    Matrix<float, 4, 4> UMatrix4x4;
    UMatrix9x1.setZero();
    UMatrix4x4.setZero();
    UMatrix9x1 = KMatrix.inverse() * FMatrix;

    
    // 1, 2, 3 1, 3, 4  2, 3, 4 1, 2, 4
    for (int Index = 0; Index < 4; Index++)
    {
        for (int Dim = 0; Dim < 3; Dim++)
        {
            if (Index == ExcludedIndex)
            {
                UMatrix4x4(Dim, Index) = 0;
            }
            else
            {
                UMatrix4x4(Dim, Index) = UMatrix9x1(3 * Index + Dim, 0);
            }
        }
    }
    for (int col = 0; col < 4; col++)
    {
        UMatrix4x4(3, col) = 1;
    }
    return UMatrix4x4;
}

Matrix<float, 9, 1> UFEMCalculateComponent::CalculateImpactForceMatrix(const FVector& InitialVelocity, const FVector& NextTickVelocity, const float Mass, const FVector& HitPoint, const TArray<FVector>& TraignleVertices)
{
    const float DeltaTime = 0.1;
    const FVector DeltaVelocity = NextTickVelocity - InitialVelocity;
    const  FVector ImpactForce = (Mass * DeltaVelocity) / DeltaTime;

    Matrix<float, 9, 1> ImpactForceMatrix;
    ImpactForceMatrix.setZero();

    // ����ġ ����� ���� �ﰢ���� �� ���� HitPosition���κ��� ������ �Ÿ��� ���.
    float TotalDistance = 0.0f;
    TArray<float> Distances = {0, 0, 0};
    for (int index = 0; index < 3; index++)
    {
        Distances[index] = (TraignleVertices[index] - HitPoint).Size();
        TotalDistance += Distances[index];
    }

    for (int i = 0; i < 3; ++i)
    {
        float Weight = 1.0f - (Distances[i] / TotalDistance);

        // �� ������ ���� x, y, z ���п� ��ݷ� �й�
        ImpactForceMatrix(i * 3 + 0, 0) = ImpactForce.X * Weight;
        ImpactForceMatrix(i * 3 + 1, 0) = ImpactForce.Y * Weight;
        ImpactForceMatrix(i * 3 + 2, 0) = ImpactForce.Z * Weight;
    }

    return ImpactForceMatrix;
}

Matrix<float, 9, 9> UFEMCalculateComponent::SubKMatrix(const Matrix<float, 12, 12> KMatrix, const FInt32Vector3 TriangleIndex)
{
    // �鿡 ���Ե� ������ �ε���
    TArray<int> DOFIndices;
    DOFIndices.SetNum(9);
    for (int i = 0; i < 3; ++i)
    {
        int VertexIndex = TriangleIndex[i];
        DOFIndices[i * 3 + 0] = VertexIndex * 3 + 0;  // x 
        DOFIndices[i * 3 + 1] = VertexIndex * 3 + 1;  // y 
        DOFIndices[i * 3 + 2] = VertexIndex * 3 + 2;  // z 
    }

    // 9x9 ���� ����� ����
    Matrix<float, 9, 9> SubMatrix;
    SubMatrix.setZero();
    // ���ü KMatrix���� �ﰢ�� ���� 9x9 ���� ��� �� ����
    for (int i = 0; i < 9; i++)
    {
        for (int j = 0; j < 9; j++)
        {
            SubMatrix(i, j) = KMatrix(DOFIndices[i], DOFIndices[j]);
        }
    }
    return SubMatrix;
}

void UFEMCalculateComponent::SetUndeformedPositions()
{
    // Vertices �迭�� �� ���� �𸮾� ������ FVector�� �޾ƿͼ�
   // undeformedPositions �迭�� x, y, z ��ǥ�� ���� �Ҵ��մϴ�.
    UndeformedPositions.SetNum(TetMeshVertices.Num() * 3); // x, y, z�� 3�� ũ��

    for (int32 i = 0; i < TetMeshVertices.Num(); ++i)
    {
        UndeformedPositions[3 * i] = TetMeshVertices[i].X;
        UndeformedPositions[3 * i + 1] = TetMeshVertices[i].Y;
        UndeformedPositions[3 * i + 2] = TetMeshVertices[i].Z;
    }
}
void UFEMCalculateComponent::KMatrix()
{
    KElements.SetNum(Tets.Num());
    // �� ���üü ���� �ݺ��� ����
    for (int32 i = 0; i < Tets.Num(); i++)
    {
        FMatrix44f Dm;;
        FMatrix44f Mininverse;
        
        // 4���� ���ؽ� �ε��� ��������
        TArray<int32> VertexIndex;
        VertexIndex.Add(Tets[i].X);
        VertexIndex.Add(Tets[i].Y);
        VertexIndex.Add(Tets[i].Z);
        VertexIndex.Add(Tets[i].W);
        
        // 4x4 ��ġ ��� Dm ����
        for (int vtx = 0; vtx < 4; vtx++)
        {
            for (int dim = 0; dim < 3; dim++)
            {
                Dm.M[dim][vtx] = UndeformedPositions[3 * VertexIndex[vtx] + dim];
            }
        }
        for (int j = 0; j < 4; j++)
        {
            Dm.M[3][j] = 1;
        }

        Mininverse = Dm.Inverse();
        Matrix<float, 6, 12> MatrixB = BMatrix(Mininverse);
        Matrix<float, 6, 6> MatrixE = EMatrix();
        Matrix<float, 6, 12> MatrixEB = MatrixE * MatrixB;
        float Volume = GetTetVolume(Tets[i]);
        KElements[i] = Volume * MatrixB.transpose() * MatrixEB;
    }
}
Matrix<float, 6, 12> UFEMCalculateComponent::BMatrix(FMatrix44f MinInverse)
{
    const TArray64<float> ArrayB =
    {
        MinInverse.M[0][0],     0,                      0,                  MinInverse.M[1][0], 0,                  0,                  MinInverse.M[2][0], 0,                  0,                  MinInverse.M[3][0], 0,                  0,
        0,                      MinInverse.M[0][1],     0,                  0,                  MinInverse.M[1][1], 0,                  0,                  MinInverse.M[2][1], 0,                  0,                  MinInverse.M[3][1], 0,
        0,                      0,                      MinInverse.M[0][2], 0,                  0,                  MinInverse.M[1][2], 0,                  0,                  MinInverse.M[2][2], 0,                  0,                   MinInverse.M[3][2],
        MinInverse.M[0][1],     MinInverse.M[0][0],     0,                  MinInverse.M[1][1], MinInverse.M[1][0], 0,                  MinInverse.M[2][1], MinInverse.M[2][0], 0,                  MinInverse.M[3][1], MinInverse.M[3][0], 0,
        0,                      MinInverse.M[0][2],     MinInverse.M[0][1], 0,                  MinInverse.M[1][2], MinInverse.M[1][1], 0,                  MinInverse.M[2][2], MinInverse.M[2][1], 0,                  MinInverse.M[3][2], MinInverse.M[3][1],
        MinInverse.M[0][2],     0,                      MinInverse.M[0][0], MinInverse.M[1][2], 0,                  MinInverse.M[1][0], MinInverse.M[2][2], 0,                  MinInverse.M[2][0], MinInverse.M[3][2], 0,                  MinInverse.M[3][0]
    };
    MatrixXf MatrixB;
    ConvertArrayToEigenMatrix(ArrayB, 6, 12, MatrixB);

    return MatrixB;
}
Matrix<float, 6, 6> UFEMCalculateComponent::EMatrix()
{
    const TArray64<float> ArrayE =
    {
       Lambda + 2 * Mu, Lambda,          Lambda,           0,  0,  0,
       Lambda,          Lambda + 2 * Mu, Lambda,           0,  0,  0,
       Lambda,          Lambda,          Lambda + 2 *  Mu, 0,  0,  0,
       0,               0,               0,                Mu, 0,  0,
       0,               0,               0,                0,  Mu, 0,
       0,               0,               0,                0,  0,  Mu
    };
    MatrixXf MatrixE;
    ConvertArrayToEigenMatrix(ArrayE, 6, 6, MatrixE);

    return MatrixE;
}
float UFEMCalculateComponent::GetTetVolume(FIntVector4 Tetra)
{
    FMatrix44f Dm;
    // 4���� ���ؽ� �ε��� ��������
    TArray<int32> VertexIndex;
    VertexIndex.Add(Tetra.X);
    VertexIndex.Add(Tetra.Y);
    VertexIndex.Add(Tetra.Z);
    VertexIndex.Add(Tetra.W);
    ;
    // 4x4 ��ġ ��� Dm ����

    for (int j = 0; j < 4; j++)
    {
        Dm.M[0][j] = 1;
    }
    for (int vtx = 0; vtx < 4; vtx++)
    {
        for (int dim = 0; dim < 3; dim++)
        {
            Dm.M[dim+1][vtx] = UndeformedPositions[3 * VertexIndex[vtx] + dim];
        }
    }
 
    float X21 = Dm.M[1][1] - Dm.M[1][0];
    float X32 = Dm.M[1][2] - Dm.M[1][1];
    float X43 = Dm.M[1][3] - Dm.M[1][2];

    float Y12 = Dm.M[2][0] - Dm.M[2][1];
    float Y23 = Dm.M[2][1] - Dm.M[2][2];
    float Y34 = Dm.M[2][2] - Dm.M[2][3];

    float Z12 = Dm.M[3][0] - Dm.M[3][1];
    float Z23 = Dm.M[3][1] - Dm.M[3][2];
    float Z34 = Dm.M[3][2] - Dm.M[3][3];

    float Vloume = X21 * (Y23 * Z34 - Y34 * Z23) + X32 * (Y34 * Z12 - Y12 * Z34) + X43 * (Y12 * Z23 - Y23 * Z12);

    return Vloume / 6;
}

void UFEMCalculateComponent::ConvertArrayToEigenMatrix(const TArray64<float>& InArray, const int32 InRows, const int32 InColumns, Eigen::MatrixXf& OutMatrix)
{
    OutMatrix.resize(InRows, InColumns);
    // Copy matrix data
    for (int32 ColumnIndex = 0; ColumnIndex < InColumns; ++ColumnIndex)
    {
        const int64 ColumnOffset = int64(ColumnIndex) * InRows;
        for (int32 RowIndex = 0; RowIndex < InRows; ++RowIndex)
        {
            OutMatrix(RowIndex, ColumnIndex) = InArray[ColumnOffset + RowIndex];
        }
    }
}
TArray<FVector3f> UFEMCalculateComponent::GetVerticesFromStaticMesh(UStaticMeshComponent* MeshComponent)
{
    TArray<FVector3f> Vertices;
    if (MeshComponent && MeshComponent->GetStaticMesh())
    {
        // ù ��° LOD�� ���� ������ ��������
        FPositionVertexBuffer* VertexBuffer = &MeshComponent->GetStaticMesh()->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;
        for (uint32 i = 0; i < VertexBuffer->GetNumVertices(); ++i)
        {
            // �� ���ؽ��� ��ġ ��������
            Vertices.Add(VertexBuffer->VertexPosition(i));
        }
    }
    return Vertices;
}


FInt32Vector4 UFEMCalculateComponent::GetClosestTriangleAndTet(const FVector& HitPosition, int32& OutExcludedIndex)
{
    // ���� ����� �ﰢ������ �Ÿ�
    float MinDistance = FLT_MAX;
    FInt32Vector4 Result = {0, 0, 0, 0};
    // �� ���ü�� ���� ���������� Ȯ��
    for (int i = 0; i < Tets.Num(); ++i)
    {
        // ���� ���ü�� 4�� ���� �ε���
        const FIntVector4& Tet = Tets[i];

        // ���ü�� 4�� ����
        FVector A = TetMeshVertices[Tet.X];
        FVector B = TetMeshVertices[Tet.Y];
        FVector C = TetMeshVertices[Tet.Z];
        FVector D = TetMeshVertices[Tet.W];
        
        // ���ü�� 4���� �ﰢ�� ���� ���� (�� ���ü�� �� ���� 3���� �������� ����)
        // 1) �ﰢ�� ABC
        FVector Tri1Point1 = A, Tri1Point2 = B, Tri1Point3 = C;
        float Distance1 = DistanceToTriangle(HitPosition, Tri1Point1, Tri1Point2, Tri1Point3);

        // 2) �ﰢ�� ABD
        FVector Tri2Point1 = A, Tri2Point2 = B, Tri2Point3 = D;
        float Distance2 = DistanceToTriangle(HitPosition, Tri2Point1, Tri2Point2, Tri2Point3);

        // 3) �ﰢ�� ACD
        FVector Tri3Point1 = A, Tri3Point2 = C, Tri3Point3 = D;
        float Distance3 = DistanceToTriangle(HitPosition, Tri3Point1, Tri3Point2, Tri3Point3);

        // 4) �ﰢ�� BCD
        FVector Tri4Point1 = B, Tri4Point2 = C, Tri4Point3 = D;
        float Distance4 = DistanceToTriangle(HitPosition, Tri4Point1, Tri4Point2, Tri4Point3);

        // ���� ����� �ﰢ���� ����, �ּڰ� ���
        TArray<float> Distances = { Distance1, Distance2, Distance3, Distance4 };
        int32 MinIndex = -1;
        FMath::Min(Distances, &MinIndex);
        float CurrentMinDistance = Distances[MinIndex];
        if (CurrentMinDistance < MinDistance)
        {
            Result[0] = i;
            MinDistance = CurrentMinDistance;
            switch (MinIndex)
            {
            case 0:
                Result[1] = 1;
                Result[2] = 2;
                Result[3] = 3;
                OutExcludedIndex = 4;
                break;
            case 1:
                Result[1] = 1;
                Result[2] = 2;
                Result[3] = 4;
                OutExcludedIndex = 3;
                break;
            case 2:
                Result[1] = 1;
                Result[2] = 3;
                Result[3] = 4;
                OutExcludedIndex = 2;
                break;
            case 3:
                Result[1] = 2;
                Result[2] = 3;
                Result[3] = 4;
                OutExcludedIndex = 1;
                break;
            }
        }
    }
    return Result;
}

float UFEMCalculateComponent::DistanceToTriangle(const FVector& OtherPoint, const FVector& PointA, const FVector& PointB, const FVector& PointC)
{
    FVector AB = PointB - PointA;
    FVector AC = PointC - PointA;
    FVector Normal = FVector::CrossProduct(AB, AC).GetSafeNormal();
    float Distance = FMath::Abs(FVector::DotProduct((OtherPoint - PointA), Normal));
    return Distance;
}

/*
FVector UFEMCalculateComponent::GetClosestPositionVertex(UStaticMeshComponent* StaticMeshComponent, FVector HitLocation)
{
    FVector ClosestVertex = {0, 0, 0};
    float MinDistance = 1000000.f;
    if (!StaticMeshComponent || !StaticMeshComponent->GetStaticMesh())
    {
        return ClosestVertex; // ��ȿ���� ���� ������Ʈ üũ
    }

    // ����ƽ �޽��� ���� ���� ��������
    const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
    if (!StaticMesh)
    {
        return ClosestVertex;
    }

    // Get Vertex Buffer
    const FStaticMeshLODResources& LODResource = StaticMesh->GetRenderData()->LODResources[0];
    const FPositionVertexBuffer& VertexBuffer = LODResource.VertexBuffers.PositionVertexBuffer;

    // ���� �� ��������
    const int32 VertexCount = VertexBuffer.GetNumVertices();

    // Hit ��ġ���� �Ÿ� ���
    for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
    {
        const FVector3f TempPosition = VertexBuffer.VertexPosition(VertexIndex);
        const FVector VertexPosition = FVector(TempPosition.X, TempPosition.Y, TempPosition.Z);
        float Distance = FVector::Distance(HitLocation, VertexPosition);

        if (Distance <= MinDistance)
        {
            MinDistance = Distance;
            ClosestVertex = VertexPosition;
        }
    }

    return ClosestVertex;
}
TArray<FVector> UFEMCalculateComponent::GetClosestTriangePositionAtHit(UStaticMeshComponent* StaticMeshComponent, const FHitResult& HitResult)
{
    TArray<FVector> TriangleVertices;

    if (!StaticMeshComponent || !StaticMeshComponent->GetStaticMesh())
    {
        return TriangleVertices; // ��ȿ���� ���� ������Ʈ üũ
    }

    const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
    if (!StaticMesh)
    {
        return TriangleVertices;
    }

    // LOD 0�� ���� �� �ε��� ���� ��������
    const FStaticMeshLODResources& LODResource = StaticMesh->GetRenderData()->LODResources[0];
    const FPositionVertexBuffer& VertexBuffer = LODResource.VertexBuffers.PositionVertexBuffer;
    const FRawStaticIndexBuffer& IndexBuffer = LODResource.IndexBuffer;

    // Hit�� �ﰢ���� FaceIndex ��������
    int32 FaceIndex = HitResult.FaceIndex;

    // ��ȿ�� FaceIndex Ȯ��
    if (FaceIndex == INDEX_NONE || FaceIndex * 3 + 2 >= IndexBuffer.GetNumIndices())
    {
        return TriangleVertices; // ��ȿ���� ���� FaceIndex üũ
    }

    // �ε��� ���ۿ��� �ﰢ���� ���� �ε��� ��������
    int32 Index0 = IndexBuffer.GetIndex(FaceIndex * 3);
    int32 Index1 = IndexBuffer.GetIndex(FaceIndex * 3 + 1);
    int32 Index2 = IndexBuffer.GetIndex(FaceIndex * 3 + 2);

    // ���� ���ۿ��� ���� ��ġ ��������
    FVector Vertex0 = FVector(VertexBuffer.VertexPosition(Index0));
    FVector Vertex1 = FVector(VertexBuffer.VertexPosition(Index1));
    FVector Vertex2 = FVector(VertexBuffer.VertexPosition(Index2));

    // �ﰢ�� ���� �迭�� �߰�
    TriangleVertices.Add(Vertex0);
    TriangleVertices.Add(Vertex1);
    TriangleVertices.Add(Vertex2);

    return TriangleVertices;
}



*/