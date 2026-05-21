// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Runtime/Manager/OptimizedSkeletalMeshWorldSubsystem.h"
#include "Runtime/Rendering/OptimizedSkeletalMeshRenderComponent.h"
#include "OptimizedSkeletalMeshDebugSpawner.generated.h"

class USkeletalMesh;
class UAnimSequence;

UCLASS(Blueprintable)
class OPTIMIZEDSKELETALMESH_API AOptimizedSkeletalMeshDebugSpawner : public AActor
{
	GENERATED_BODY()

public:
#pragma region Lifecycle
	AOptimizedSkeletalMeshDebugSpawner();

	virtual void OnConstruction(const FTransform& InTransform) override;
	virtual void Tick(float InDeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type InEndPlayReason) override;
#pragma endregion

#pragma region Debug
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Optimized Skeletal Mesh|Debug")
	void RebuildInstances();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Optimized Skeletal Mesh|Debug")
	void ClearInstances();
#pragma endregion

#pragma region Settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Debug")
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Animation")
	TObjectPtr<UAnimSequence> Animation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Animation")
	float AnimationStartTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Animation")
	float AnimationPlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Animation")
	bool bLoopAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Animation")
	bool bPlayAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Animation", meta = (ClampMin = "0.0"))
	float AnimationUpdateRateHz = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Debug", meta = (ClampMin = "1"))
	int32 CountX = 25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Debug", meta = (ClampMin = "1"))
	int32 CountY = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Debug", meta = (ClampMin = "1.0"))
	float Spacing = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Debug")
	bool bRebuildOnConstruction = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Debug")
	bool bDrawDebugBounds = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Debug")
	bool bDrawMeshSections = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Debug")
	EOptimizedSkeletalMeshDrawMode MeshDrawMode = EOptimizedSkeletalMeshDrawMode::DynamicMeshProof;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Debug")
	bool bAutoLOD = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Debug", meta = (ClampMin = "0", EditCondition = "!bAutoLOD"))
	int32 ForcedLODIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Debug", meta = (ClampMin = "0"))
	int32 MaxMeshDrawInstances = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Culling")
	bool bEnableInstanceFrustumCulling = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Culling", meta = (ClampMin = "1.0"))
	float InstanceCullBoundsScale = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Culling")
	bool bUseConservativeProxyBounds = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Culling", meta = (ClampMin = "1000.0"))
	float ConservativeProxyBoundsExtent = 10000000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Culling")
	bool bDrawCullingDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Culling", meta = (EditCondition = "bDrawCullingDebug"))
	bool bDrawCullTestBounds = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Debug")
	int32 SpawnedInstanceCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Debug")
	int32 VisibleRenderBatchCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	FOptimizedSkeletalMeshRenderStats LastRenderStats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	FOptimizedSkeletalMeshAnimationStats LastAnimationStats;
#pragma endregion

private:
#pragma region Helpers
	UOptimizedSkeletalMeshWorldSubsystem* GetOptimizedSubsystem() const;
	FTransform GetInstanceTransform(int32 InX, int32 InY) const;
#pragma endregion

#pragma region RuntimeState
	UPROPERTY(VisibleAnywhere, Category = "Optimized Skeletal Mesh|Debug")
	TObjectPtr<UOptimizedSkeletalMeshRenderComponent> PreviewRenderComponent = nullptr;

	UPROPERTY(Transient)
	TArray<FOptimizedSkeletalMeshInstanceHandle> SpawnedHandles;
#pragma endregion
};
