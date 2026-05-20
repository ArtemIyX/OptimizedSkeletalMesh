// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Runtime/Manager/OptimizedSkeletalMeshWorldSubsystem.h"
#include "OptimizedSkeletalMeshDebugSpawner.generated.h"

class USkeletalMesh;
class UOptimizedSkeletalMeshRenderComponent;

UCLASS(Blueprintable)
class OPTIMIZEDSKELETALMESH_API AOptimizedSkeletalMeshDebugSpawner : public AActor
{
	GENERATED_BODY()

public:
	AOptimizedSkeletalMeshDebugSpawner();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Optimized Skeletal Mesh|Debug")
	void RebuildInstances();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Optimized Skeletal Mesh|Debug")
	void ClearInstances();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Debug")
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Debug", meta = (ClampMin = "1"))
	int32 CountX = 25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Debug", meta = (ClampMin = "1"))
	int32 CountY = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Debug", meta = (ClampMin = "1.0"))
	float Spacing = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Debug")
	bool bRebuildOnConstruction = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Debug")
	int32 SpawnedInstanceCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Debug")
	int32 VisibleRenderBatchCount = 0;

private:
	UOptimizedSkeletalMeshWorldSubsystem* GetOptimizedSubsystem() const;
	FTransform GetInstanceTransform(int32 X, int32 Y) const;

	UPROPERTY(VisibleAnywhere, Category = "Optimized Skeletal Mesh|Debug")
	TObjectPtr<UOptimizedSkeletalMeshRenderComponent> PreviewRenderComponent = nullptr;

	UPROPERTY(Transient)
	TArray<FOptimizedSkeletalMeshInstanceHandle> SpawnedHandles;
};
