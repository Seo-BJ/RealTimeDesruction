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
	TArray<FVector3f> Vertices;
	TArray<FVector3f> Sites;
	TArray<FVector3f> BaryCenters;
	TArray<int32> Region;
	void Lloyd_Algo();
	void GetVertexDataFromStaticMeshComponent(const UStaticMeshComponent* StaticMeshComponent);
	void SetVoronoiSites(TArray<FVector3f> VoronoiSites);
	~CVT();

private:
	void RefreshRegion();
	void CalculateCentroids();
	TArray<FVector3f> GenerateNewSite();
	bool isEqualSites(TArray<FVector3f>& Sites1, TArray<FVector3f>& Sites2);
};
