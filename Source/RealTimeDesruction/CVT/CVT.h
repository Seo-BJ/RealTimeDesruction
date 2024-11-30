// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Engine/StaticMesh.h"
#include "Misc/ScopeLock.h"

/**
 * ����
 * Vertices : ���ؽ� ��ǥ ����
 * Sites : �õ�����Ʈ ���ؽ� �ε��� ����
 * BaryCenters : �����߽� ��ǥ ����
 * Region : ���γ��� ���� �ǹ�. �� ���ؽ��� ���° �õ�����Ʈ�� ���� ������� ����
 * 
 * �Լ�
 * Lloyd_Algo : ���̵� �˰����
 * GetVertexDataFromStaticMeshComponent : ����ƽ �޽� ������Ʈ���� ���ؽ� ���� ������
 * RefreshRegion : ���� �õ�����Ʈ�� �������� ���γ��� ���� �缳��
 * CalculateCentroids : �� ���γ��� ���� �����߽� ���
 * GenerateNewSite : �����߽��� �������� ���ο� �õ� ����Ʈ ���� 
 * isEqualSites : �� �õ�����Ʈ TARRAY�� �������� Ȯ��
 */
class REALTIMEDESRUCTION_API CVT
{
public:
	CVT();
	TArray<FVector> Vertices;
	TArray<uint32> Sites;
	TArray<FVector> BaryCenters;
	TArray<uint32> Region;
	void Lloyd_Algo();
	void GetVertexDataFromStaticMeshComponent(const UStaticMeshComponent* StaticMeshComponent);
	void SetVertices(TArray<FVector> new_Vertices);
	void SetVoronoiSites(TArray<uint32> VoronoiSites);
	~CVT();

private:
	void RefreshRegion();
	void CalculateCentroids();
	TArray<uint32> GenerateNewSite();
	bool isEqualSites(TArray<uint32>& Sites1, TArray<uint32>& Sites2);
};
