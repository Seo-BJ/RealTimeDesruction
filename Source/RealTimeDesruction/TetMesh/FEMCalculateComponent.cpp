// Fill out your copyright notice in the Description page of Project Settings.


#include "FEMCalculateComponent.h"

#include "FTetWildWrapper.h"
#include "Engine/StaticMesh.h"

#include "FTetWildWrapper.h"

using namespace Eigen;

UFEMCalculateComponent::UFEMCalculateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

}
void UFEMCalculateComponent::BeginPlay()
{
	Super::BeginPlay();
    InitializeTetMesh();
}
void UFEMCalculateComponent::InitializeTetMesh()
{
    // StaticMesh로 부터
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

        GenerateGraphFromTets();

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

void UFEMCalculateComponent::GenerateGraphFromTets()
{
    for (uint32 i = 0; i < (uint32)TetMeshVertices.Num(); i++)
        Graph.addVertex(i);

    for (const FIntVector4 Tet : Tets)
    {
        FVector Vertex_X = TetMeshVertices[Tet.X];
        FVector Vertex_Y = TetMeshVertices[Tet.Y];
        FVector Vertex_Z = TetMeshVertices[Tet.Z];
        FVector Vertex_W = TetMeshVertices[Tet.W];

        Graph.addLink(Tet.X, Tet.Y, Vertex_Y - Vertex_X);
        Graph.addLink(Tet.Y, Tet.Z, Vertex_Z - Vertex_Y);
        Graph.addLink(Tet.Z, Tet.X, Vertex_X - Vertex_Z);
        Graph.addLink(Tet.X, Tet.W, Vertex_W - Vertex_X);
        Graph.addLink(Tet.Y, Tet.W, Vertex_W - Vertex_Y);
        Graph.addLink(Tet.Z, Tet.W, Vertex_W - Vertex_Z);
    }
}

float UFEMCalculateComponent::CalculateEnergyAtTatUsingFEM(const FVector& Velocity, const FVector& NextTickVelocity, const float Mass, const FVector& HitPoint)
{
    int32 ExcludedIndex = 0;
    FInt32Vector4 ClosestResult = GetClosestTriangleAndTet(HitPoint, ExcludedIndex);
    int32 TargetTetIndex = ClosestResult[0];

    CurrentImpactPoint = { 
        (uint32)Tets[TargetTetIndex][ClosestResult[1] - 1],
        (uint32)Tets[TargetTetIndex][ClosestResult[2] - 1],
        (uint32)Tets[TargetTetIndex][ClosestResult[3] - 1]
    };

    // Hit했을 때 분석을 할 삼각형 면 정점
    FVector A = TetMeshVertices[CurrentImpactPoint[0]];
    FVector B = TetMeshVertices[CurrentImpactPoint[1]];
    FVector C = TetMeshVertices[CurrentImpactPoint[2]];

    Matrix<float, 9, 1> F = CalculateImpactForceMatrix(Velocity, NextTickVelocity, Mass, HitPoint, { A, B, C });
    Matrix<float, 9, 9> K = SubKMatrix(KElements[TargetTetIndex], ExcludedIndex);
    Matrix<float, 4, 4> u = UMatrix(K, F, { ClosestResult[1], ClosestResult[2], ClosestResult[3] }, ExcludedIndex);

    //LogMatrix<Matrix<float, 12, 12>>(KElements[TargetTetIndex], "Origin Matrix K");
    //LogMatrix<Matrix<float, 9, 1>>(F, "F");
    //LogMatrix<Matrix<float, 9, 9>>(K, "Sub Matrix K");
    //LogMatrix<Matrix<float, 4, 4>>(u, "u");
    //UE_LOG(LogTemp, Warning, TEXT("Excluded Index = %d"), ExcludedIndex);
    //UE_LOG(LogTemp, Warning, TEXT("K Determinant : %f"), K.determinant());

    Matrix<float, 4, 4> Dm;
    Matrix<float, 3, 4> Dm2;
    TArray<int32> VertexIndex;
    VertexIndex.Add(Tets[TargetTetIndex].X);
    VertexIndex.Add(Tets[TargetTetIndex].Y);
    VertexIndex.Add(Tets[TargetTetIndex].Z);
    VertexIndex.Add(Tets[TargetTetIndex].W);

    // 4x4 위치 행렬 Dm 생성
    for (int vtx = 0; vtx < 4; vtx++)
    {
        for (int dim = 0; dim < 3; dim++)
        {
            Dm(dim, vtx) = UndeformedPositions[3 * VertexIndex[vtx] + dim]/ 100;
            Dm2(dim, vtx) = UndeformedPositions[3 * VertexIndex[vtx] + dim]/ 100;
        }
    }
    for (int j = 0; j < 4; j++)
    {
        Dm(3, j) = 1;
    }
    Matrix<float, 3, 3> Jaco = Jacobian(Dm2);
    float TargetTetVolume = GetTetVolume(Jaco);
    //UE_LOG(LogTemp, Warning, TEXT("Target Tet Volume : %f"), TargetTetVolume);
    return CalculateEnergy(Dm, u, TargetTetVolume);
}



float UFEMCalculateComponent::CalculateEnergy(Matrix<float, 4, 4> DmMatrix, Matrix<float, 4, 4> UMatrix, float TetVolume)
{
    Matrix<float, 4, 4> DsMatrix = DmMatrix + UMatrix;
    //LogMatrix<Matrix<float, 4, 4>>(DmMatrix, "Dm Matrix");
    //LogMatrix<Matrix<float, 4, 4>>(UMatrix, "U Matrix");
    //LogMatrix<Matrix<float, 4, 4>>(DsMatrix, "Ds Matrix");

    Matrix<float, 4, 4> GMatrix = DsMatrix * DmMatrix.inverse();
    //LogMatrix<Matrix<float, 4, 4>>(GMatrix, "GMatrix");

    Matrix<float, 4, 4> Identity;
    Identity.setZero();
    Identity(0, 0) = 1;
    Identity(1, 1) = 1;
    Identity(2, 2) = 1;
    Identity(3, 3) = 1;
    //LogMatrix<Matrix<float, 4, 4>>(Identity, "Identity Matrix");
    Matrix<float, 4, 4> StrainTensorMatrix = (GMatrix + GMatrix.transpose()) / 2 - Identity;
    //LogMatrix<Matrix<float, 4, 4>>(StrainTensorMatrix, "Strain Tensor Matrix");
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
   //LogMatrix<Matrix<float, 9, 1>>(UMatrix9x1, "U Matrix 9 x 1");
   //LogMatrix<Matrix<float, 9, 9>>(KMatrix.inverse(), "K Inverse");
   UE_LOG(LogTemp, Warning, TEXT("Sub K Determinant : %f"), KMatrix.determinant());
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
                UMatrix4x4(Dim, Index) = UMatrix9x1(3 * Index + Dim, 0) / 100;
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
    const float DeltaTime = 0.01;
    const FVector DeltaVelocity = NextTickVelocity - InitialVelocity;
    const  FVector ImpactForce = (Mass * DeltaVelocity) / DeltaTime;

    Matrix<float, 9, 1> ImpactForceMatrix;
    ImpactForceMatrix.setZero();

    // 가중치 계산을 위해 삼각형의 각 점과 HitPosition으로부터 떨어진 거리를 계산.
    TArray<float> Distances = {0, 0, 0};
    for (int index = 0; index < 3; index++)
    {
        Distances[index] = (TraignleVertices[index] - HitPoint).Size();
    }
    float WeightSum = 0.f;
    for (int i = 0; i < 3; ++i)
    {
        float Weight = 1/Distances[i] * (Distances[0]* Distances[1]* Distances[2])/(Distances[0]* Distances[1] + Distances[1]* Distances[2] + Distances[0]* Distances[2]);

        // 각 정점에 대한 x, y, z 성분에 충격력 분배
        ImpactForceMatrix(i * 3 + 0, 0) = ImpactForce.X * Weight;
        ImpactForceMatrix(i * 3 + 1, 0) = ImpactForce.Y * Weight;
        ImpactForceMatrix(i * 3 + 2, 0) = ImpactForce.Z * Weight;
        WeightSum += Weight;
    }
    UE_LOG(LogTemp, Warning, TEXT("Weigh Sum = %f"), WeightSum);
    return ImpactForceMatrix;
}

Matrix<float, 9, 9> UFEMCalculateComponent::SubKMatrix(const Matrix<float, 12, 12> KMatrix, const int32 ExcludedIndex)
{
    // To do: Triangle Index가 1, 2, 3, 4로 이루어져 있음. (0부터 시작하지 않음.) 수정 필요??

    // 9x9 서브 행렬을 생성
    Matrix<float, 9, 9> SubMatrix;
    SubMatrix.setZero();
    // 사면체 KMatrix에서 삼각형 면의 9x9 서브 행렬 값 추출
    for (int i = 0; i < 9; i++)
    {
        for (int j = 0; j < 9; j++)
        {
            int TargetRow = i;
            int TargetCol = j;
            if (i >= (ExcludedIndex- 1) * 3)
            {
                TargetRow += 3;
            }
            if (j >= (ExcludedIndex-1) * 3)
            {
                TargetCol += 3;
            }
            SubMatrix(i, j) = KMatrix(TargetRow, TargetCol);
        }
    }
    return SubMatrix;
}

void UFEMCalculateComponent::SetUndeformedPositions()
{
    // Vertices 배열의 각 점을 언리얼 엔진의 FVector로 받아와서
   // undeformedPositions 배열에 x, y, z 좌표를 각각 할당합니다.
    UndeformedPositions.SetNum(TetMeshVertices.Num() * 3); // x, y, z로 3배 크기

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
    // 각 사면체체 마다 반복문 실행
    for (int32 i = 0; i < Tets.Num(); i++)
    {
        Matrix<float, 3, 4> Demention;
        Matrix<float, 4, 4> MinInverse;
        // 4개의 버텍스 인덱스 가져오기
        TArray<int32> VertexIndex;
        VertexIndex.Add(Tets[i].X);
        VertexIndex.Add(Tets[i].Y);
        VertexIndex.Add(Tets[i].Z);
        VertexIndex.Add(Tets[i].W);
        
        // 3x4 위치 행렬 Dm 생성
        for (int vtx = 0; vtx < 4; vtx++)
        {
            for (int dim = 0; dim < 3; dim++)
            {
                Demention(dim, vtx) = UndeformedPositions[3 * VertexIndex[vtx] + dim];
            }
        }
        // 자코비안 행렬 계산
        Matrix<float, 3, 3> Jaco = Jacobian(Demention);
        //LogMatrix<Matrix<float, 3, 3>>(Jaco, "Jacobian 3 x 3");
        //UE_LOG(LogTemp, Warning, TEXT("Jaco Determinant : %f"), Jaco.determinant());
        // 형상함수 행렬 계산.
        Matrix<float, 4, 3> ShapeFunctionDiffMatrix;
        ShapeFunctionDiffMatrix.setZero();
        ShapeFunctionDiffMatrix(0, 0) = -1;
        ShapeFunctionDiffMatrix(0, 1) = -1;
        ShapeFunctionDiffMatrix(0, 2) = -1;
        ShapeFunctionDiffMatrix(1, 0) = 1;
        ShapeFunctionDiffMatrix(2, 1) = 1;
        ShapeFunctionDiffMatrix(3, 2) = 1;
        // 최종 형상함수를 각 x, y, z 성분에 대한 미분 행렬 계산.
        Matrix<float, 4, 3> Result = ShapeFunctionDiffMatrix * Jaco.inverse();
        float Volume = GetTetVolume(Jaco);
        //UE_LOG(LogTemp, Warning, TEXT("Volume : %f"), Volume);
        Matrix<float, 6, 12> MatrixB = BMatrix(Result)/ (Volume);
        Matrix<float, 6, 6> MatrixE = EMatrix();

        KElements[i] = Volume * MatrixB.transpose() * MatrixE * MatrixB;
    }
}
Matrix<float, 6, 12> UFEMCalculateComponent::BMatrix(Matrix<float, 4, 3> Matrix)
{
    const TArray64<float> ArrayB =
    {
        Matrix(0,0), 0, 0, Matrix(1,0), 0, 0, Matrix(2,0), 0, 0,  Matrix(3,0), 0, 0,
        0, Matrix(0,1), 0, 0,  Matrix(1,1), 0, 0,  Matrix(2,1), 0, 0, Matrix(3,1), 0,
        0, 0, Matrix(0,2), 0, 0,  Matrix(1,2), 0,0, Matrix(2,2), 0, 0, Matrix(3,2),
        Matrix(0,1), Matrix(0,2), 0, Matrix(1,1), Matrix(1,0), 0, Matrix(2,1), Matrix(2,0), 0, Matrix(3,1), Matrix(3,0), 0,
        0,  Matrix(0,2), Matrix(0,1), 0, Matrix(1,2),  Matrix(1,1), 0,  Matrix(2,2), Matrix(2,1), 0,Matrix(3,2), Matrix(3,1),
         Matrix(0,2), 0, Matrix(0,0),  Matrix(1,2), 0, Matrix(1,0), Matrix(2,2), 0, Matrix(2,0),  Matrix(3,2), 0, Matrix(3,0)
    };
    MatrixXf MatrixB;
    ConvertArrayToEigenMatrix(ArrayB, 6, 12, MatrixB);

    return MatrixB;
}
Matrix<float, 3, 3> UFEMCalculateComponent::Jacobian(Matrix<float, 3, 4> PositionMatrix)
{
    Matrix<float, 3, 3> Result;
    Result.setZero();
    Result(0, 0) = PositionMatrix(0, 1) - PositionMatrix(0, 0);
    Result(0, 1) = PositionMatrix(0, 2) - PositionMatrix(0, 0);
    Result(0, 2) = PositionMatrix(0, 3) - PositionMatrix(0, 0);
    Result(1, 0) = PositionMatrix(1, 1) - PositionMatrix(1, 0);
    Result(1, 1) = PositionMatrix(1, 2) - PositionMatrix(1, 0);
    Result(1, 2) = PositionMatrix(1, 3) - PositionMatrix(1, 0);
    Result(2, 0) = PositionMatrix(2, 1) - PositionMatrix(2, 0);
    Result(2, 1) = PositionMatrix(2, 2) - PositionMatrix(2, 0);
    Result(2, 2) = PositionMatrix(2, 3) - PositionMatrix(2, 0);

    return Result / 100;
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
float UFEMCalculateComponent::GetTetVolume(Matrix<float, 3, 3> Jaco)
{
    return Jaco.determinant() / 6;
    /*
    FMatrix44f Dm;
    // 4개의 버텍스 인덱스 가져오기
    TArray<int32> VertexIndex;
    VertexIndex.Add(Tetra.X);
    VertexIndex.Add(Tetra.Y);
    VertexIndex.Add(Tetra.Z);
    VertexIndex.Add(Tetra.W);
    ;
    // 4x4 위치 행렬 Dm 생성

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
    */
    
}

void UFEMCalculateComponent::ConvertArrayToEigenMatrix(const TArray64<float>& InArray, const int32 InRows, const int32 InColumns, Eigen::MatrixXf& OutMatrix)
{
    // Resize the output matrix to match the input dimensions
    OutMatrix.resize(InRows, InColumns);

    // Copy matrix data from InArray to Eigen matrix
    for (int32 RowIndex = 0; RowIndex < InRows; ++RowIndex)
    {
        for (int32 ColumnIndex = 0; ColumnIndex < InColumns; ++ColumnIndex)
        {
            // InArray is a 1D array, so access the correct element for the (RowIndex, ColumnIndex)
            OutMatrix(RowIndex, ColumnIndex) = InArray[ColumnIndex * InRows + RowIndex];
        }
    }
}
TArray<FVector3f> UFEMCalculateComponent::GetVerticesFromStaticMesh(UStaticMeshComponent* MeshComponent)
{
    TArray<FVector3f> Vertices;
    if (MeshComponent && MeshComponent->GetStaticMesh())
    {
        // 첫 번째 LOD의 렌더 데이터 가져오기
        FPositionVertexBuffer* VertexBuffer = &MeshComponent->GetStaticMesh()->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;
        for (uint32 i = 0; i < VertexBuffer->GetNumVertices(); ++i)
        {
            // 각 버텍스의 위치 가져오기
            Vertices.Add(VertexBuffer->VertexPosition(i));
        }
    }
    return Vertices;
}


FInt32Vector4 UFEMCalculateComponent::GetClosestTriangleAndTet(const FVector& HitPosition, int32& OutExcludedIndex)
{
    // 가장 가까운 사면체, 삼각형과의 거리
    float TetMinDistance = FLT_MAX;
    float TriMinDistance = FLT_MAX;
    FInt32Vector4 Result = {0, 0, 0, 0};
    // 각 사면체에 대해 순차적으로 확인
    for (int i = 0; i < Tets.Num(); ++i)
    {
        // 현재 사면체의 4개 정점 인덱스
        const FIntVector4& Tet = Tets[i];

        // 사면체의 4개 정점
        FVector A = TetMeshVertices[Tet.X];
        FVector B = TetMeshVertices[Tet.Y];
        FVector C = TetMeshVertices[Tet.Z];
        FVector D = TetMeshVertices[Tet.W];

        // 먼저 사면체 거리 측정
        FVector Center = (A + B + C + D) / 4;
        float CurrentTetMinDistance = FVector::Dist(Center, HitPosition);
        if (CurrentTetMinDistance < TetMinDistance)
        {
            TetMinDistance = CurrentTetMinDistance;

            // 사면체는 4개의 삼각형 면을 가짐 (각 사면체의 각 면은 3개의 정점으로 구성)
            // 1) 삼각형 ABC
            FVector Tri1Point1 = A, Tri1Point2 = B, Tri1Point3 = C;
            float Distance1 = DistanceToTriangle(HitPosition, Tri1Point1, Tri1Point2, Tri1Point3);

            // 2) 삼각형 ABD
            FVector Tri2Point1 = A, Tri2Point2 = B, Tri2Point3 = D;
            float Distance2 = DistanceToTriangle(HitPosition, Tri2Point1, Tri2Point2, Tri2Point3);

            // 3) 삼각형 ACD
            FVector Tri3Point1 = A, Tri3Point2 = C, Tri3Point3 = D;
            float Distance3 = DistanceToTriangle(HitPosition, Tri3Point1, Tri3Point2, Tri3Point3);

            // 4) 삼각형 BCD
            FVector Tri4Point1 = B, Tri4Point2 = C, Tri4Point3 = D;
            float Distance4 = DistanceToTriangle(HitPosition, Tri4Point1, Tri4Point2, Tri4Point3);

            // 가장 가까운 삼각형을 선택, 최솟값 계산
            TArray<float> Distances = { Distance1, Distance2, Distance3, Distance4 };
            int32 MinIndex = -1;
            FMath::Min(Distances, &MinIndex);
            float CurrentTriMinDistance = Distances[MinIndex];
            if (CurrentTriMinDistance < TriMinDistance)
            {
                Result[0] = i;
                TriMinDistance = CurrentTriMinDistance;
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
        return ClosestVertex; // 유효하지 않은 컴포넌트 체크
    }

    // 스태틱 메시의 정점 정보 가져오기
    const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
    if (!StaticMesh)
    {
        return ClosestVertex;
    }

    // Get Vertex Buffer
    const FStaticMeshLODResources& LODResource = StaticMesh->GetRenderData()->LODResources[0];
    const FPositionVertexBuffer& VertexBuffer = LODResource.VertexBuffers.PositionVertexBuffer;

    // 정점 수 가져오기
    const int32 VertexCount = VertexBuffer.GetNumVertices();

    // Hit 위치와의 거리 계산
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
        return TriangleVertices; // 유효하지 않은 컴포넌트 체크
    }

    const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh();
    if (!StaticMesh)
    {
        return TriangleVertices;
    }

    // LOD 0의 정점 및 인덱스 버퍼 가져오기
    const FStaticMeshLODResources& LODResource = StaticMesh->GetRenderData()->LODResources[0];
    const FPositionVertexBuffer& VertexBuffer = LODResource.VertexBuffers.PositionVertexBuffer;
    const FRawStaticIndexBuffer& IndexBuffer = LODResource.IndexBuffer;

    // Hit된 삼각형의 FaceIndex 가져오기
    int32 FaceIndex = HitResult.FaceIndex;

    // 유효한 FaceIndex 확인
    if (FaceIndex == INDEX_NONE || FaceIndex * 3 + 2 >= IndexBuffer.GetNumIndices())
    {
        return TriangleVertices; // 유효하지 않은 FaceIndex 체크
    }

    // 인덱스 버퍼에서 삼각형의 정점 인덱스 가져오기
    int32 Index0 = IndexBuffer.GetIndex(FaceIndex * 3);
    int32 Index1 = IndexBuffer.GetIndex(FaceIndex * 3 + 1);
    int32 Index2 = IndexBuffer.GetIndex(FaceIndex * 3 + 2);

    // 정점 버퍼에서 정점 위치 가져오기
    FVector Vertex0 = FVector(VertexBuffer.VertexPosition(Index0));
    FVector Vertex1 = FVector(VertexBuffer.VertexPosition(Index1));
    FVector Vertex2 = FVector(VertexBuffer.VertexPosition(Index2));

    // 삼각형 정점 배열에 추가
    TriangleVertices.Add(Vertex0);
    TriangleVertices.Add(Vertex1);
    TriangleVertices.Add(Vertex2);

    return TriangleVertices;
}



*/