// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Engine/StaticMesh.h"

/**
 * ����
 * Vertices : ���ؽ� ��ǥ ����
 * Sites : �õ�����Ʈ ��ǥ ����
 * BaryCenters : �����߽� ��ǥ ����
 * Region : ���γ��� ���� �ǹ�. �� ���ؽ��� ���° �õ�����Ʈ�� ���� ������� ����
 * 
 * �Լ�
 * Lloyd_Algo : ���̵� �˰���
 * GetVertexDataFromStaticMeshComponent : ����ƽ �޽� ������Ʈ���� ���ؽ� ���� ������
 * RefreshRegion : ���� �õ�����Ʈ�� �������� ���γ��� ���� �缳��
 * CalculateCentroids : �� ���γ��� ���� �����߽� ���
 * GenerateNewSite : �����߽��� �������� ���ο� �õ� ����Ʈ ���� 
 * isEqualSites : �� �õ�����Ʈ TARRAY�� �������� Ȯ��
 */
class CVT
{
public:
	CVT();
	TArray<FVector> Vertices;
	TArray<FVector> Sites;
	TArray<FVector> BaryCenters;
	TArray<int32> Region;
	void Lloyd_Algo(TArray<FVector>& InputSites);
	~CVT();

private:
	void GetVertexDataFromStaticMeshComponent(const UStaticMeshComponent* StaticMeshComponent);
	void RefreshRegion();
	void CalculateCentroids();
	TArray<FVector> GenerateNewSite();
	bool isEqualSites(TArray<FVector>& Sites1, TArray<FVector>& Sites2);
};
