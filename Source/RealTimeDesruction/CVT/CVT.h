// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Engine/StaticMesh.h"
#include "Misc/ScopeLock.h"

/**
 * 변수
 * Vertices : 버텍스 좌표 저장
 * Sites : 시드포인트 좌표 저장
 * BaryCenters : 무게중심 좌표 저장
 * Region : 보로노이 셀을 의미. 각 버텍스가 몇번째 시드포인트가 제일 가까운지 저장
 * 
 * 함수
 * Lloyd_Algo : 로이드 알고리즘
 * GetVertexDataFromStaticMeshComponent : 스태틱 메쉬 컴포넌트에서 버텍스 정보 가져옴
 * RefreshRegion : 현재 시드포인트를 기준으로 보로노이 셀을 재설정
 * CalculateCentroids : 각 보로노이 셀의 무게중심 계산
 * GenerateNewSite : 무게중심을 기준으로 새로운 시드 포인트 생성 
 * isEqualSites : 두 시드포인트 TARRAY가 동일한지 확인
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
