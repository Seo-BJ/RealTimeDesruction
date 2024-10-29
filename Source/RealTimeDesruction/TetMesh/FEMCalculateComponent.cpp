// Fill out your copyright notice in the Description page of Project Settings.


#include "FEMCalculateComponent.h"

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
	
}


void UFEMCalculateComponent::CreateDm(const TArray<FVector>& Vertices, TArray<float>& undeformedPositions)
{
    // Vertices 배열의 각 점을 언리얼 엔진의 FVector로 받아와서
   // undeformedPositions 배열에 x, y, z 좌표를 각각 할당합니다.
    undeformedPositions.SetNum(Vertices.Num() * 3); // x, y, z로 3배 크기

    for (int32 i = 0; i < Vertices.Num(); ++i)
    {
        undeformedPositions[3 * i] = Vertices[i].X;
        undeformedPositions[3 * i + 1] = Vertices[i].Y;
        undeformedPositions[3 * i + 2] = Vertices[i].Z;
    }
}

void UFEMCalculateComponent::BuildMatrixK(const TArray<float> UndeformedPositions, const TArray<FIntVector4> Tets, TArray<FMatrix>& KElements, float YoungsModulus, float PoissonsRatio)
{

     // 각 사면체의 역행렬을 저장할 배열

    TArray<FMatrix44f> MininverseArray;
    TArray<Matrix<float, 12, 12>> KElement;
    // 각 사면체체 마다 반복문 실행
    for (int32 i = 0; i < Tets.Num(); i++)
    {
        FMatrix44f Dm;;
        FMatrix44f Mininverse;
        
        // 4개의 버텍스 인덱스 가져오기
        TArray<int32> VertexIndex;
        VertexIndex.Add(Tets[i].X);
        VertexIndex.Add(Tets[i].Y);
        VertexIndex.Add(Tets[i].Z);
        VertexIndex.Add(Tets[i].W);
        
        // 4x4 위치 행렬 Dm 생성
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
        MininverseArray.Add(Mininverse);
    }
    for (int32 i = 0; i < Tets.Num(); i++)
    {
        Matrix<float, 6, 12> MatrixB = BuildMatrixB(MininverseArray[i]);
        Matrix<float, 6, 6> MatrixE = BuildMatrixE(MaterialLambda, MaterialMu);
        Matrix<float, 6, 12> MatrixEB = MatrixE * MatrixB;
        float Volume = GetTetraVolume(Tets[i], UndeformedPositions);
        KElement[i] = Volume * MatrixB.transpose() * MatrixEB;
    }
}

Matrix<float, 6, 12> UFEMCalculateComponent::BuildMatrixB(FMatrix44f MinInverse)
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

Matrix<float, 6, 6> UFEMCalculateComponent::BuildMatrixE(float Lambda, float Mu)
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

float UFEMCalculateComponent::GetTetraVolume(FIntVector4 Tetra, const TArray<float> UndeformedPositions)
{

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
        for (int dim = 1; dim < 4; dim++)
        {
            Dm.M[dim][vtx] = UndeformedPositions[3 * VertexIndex[vtx] + dim];
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

