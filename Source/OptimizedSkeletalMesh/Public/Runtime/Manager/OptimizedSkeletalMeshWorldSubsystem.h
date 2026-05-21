// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoneContainer.h"
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OptimizedSkeletalMeshWorldSubsystem.generated.h"

class USkeletalMesh;
class UAnimSequence;
class AActor;
class UOptimizedSkeletalMeshRenderComponent;

struct FOptimizedSkeletalMeshAnimationMeshCache
{
	TArray<FBoneIndexType> RequiredBoneIndices;
	FBoneContainer RequiredBones;
	TArray<FMatrix44f> RefBasesInvMatrices;
};

USTRUCT(BlueprintType)
struct OPTIMIZEDSKELETALMESH_API FOptimizedSkeletalMeshInstanceHandle
{
	GENERATED_BODY()

public:
	FOptimizedSkeletalMeshInstanceHandle() = default;
	explicit FOptimizedSkeletalMeshInstanceHandle(const int32 InId)
		: Id(InId)
	{
	}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh")
	int32 Id = INDEX_NONE;

	bool IsValid() const
	{
		return Id != INDEX_NONE;
	}
};

USTRUCT(BlueprintType)
struct OPTIMIZEDSKELETALMESH_API FOptimizedSkeletalMeshInstanceDesc
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh")
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh")
	FTransform WorldTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh", meta = (ClampMin = "0"))
	int32 LODIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh")
	bool bAutoLOD = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Animation")
	TObjectPtr<UAnimSequence> Animation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh")
	float AnimationTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Animation")
	float AnimationPlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Animation")
	bool bLoopAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Animation")
	bool bPlayAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Animation", meta = (ClampMin = "0.0"))
	float AnimationUpdateRateHz = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh")
	bool bVisible = true;
};

USTRUCT(BlueprintType)
struct OPTIMIZEDSKELETALMESH_API FOptimizedSkeletalMeshAnimationStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	int32 RegisteredInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	int32 AnimatedInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	int32 ActiveAnimationInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	int32 DirtyAnimationInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	int32 SkippedUpdateRateInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	int32 ParallelPoseBatches = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	int32 AdvancedInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	int32 FinishedInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	int32 PoseEvaluatedInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	int32 FailedPoseEvaluations = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	int32 BonePaletteInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	int32 TotalBoneMatrices = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	int32 MaxBonesPerInstance = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	float LastDeltaTime = 0.0f;
};

USTRUCT(BlueprintType)
struct OPTIMIZEDSKELETALMESH_API FOptimizedSkeletalMeshInstanceSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh")
	FOptimizedSkeletalMeshInstanceHandle Handle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh")
	FOptimizedSkeletalMeshInstanceDesc Desc;
};

struct FOptimizedSkeletalMeshBonePaletteSnapshot
{
	FOptimizedSkeletalMeshInstanceHandle Handle;
	TArray<FMatrix44f> BonePalette;
};

UCLASS()
class OPTIMIZEDSKELETALMESH_API UOptimizedSkeletalMeshWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& InCollection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float InDeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	FOptimizedSkeletalMeshInstanceHandle RegisterInstance(const FOptimizedSkeletalMeshInstanceDesc& InDesc);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool UnregisterInstance(FOptimizedSkeletalMeshInstanceHandle InHandle);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool UpdateInstance(FOptimizedSkeletalMeshInstanceHandle InHandle, const FOptimizedSkeletalMeshInstanceDesc& InDesc);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool UpdateInstanceTransform(FOptimizedSkeletalMeshInstanceHandle InHandle, const FTransform& InWorldTransform);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool UpdateInstanceAnimationTime(FOptimizedSkeletalMeshInstanceHandle InHandle, float InAnimationTime);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool SetInstanceAnimationPlaying(FOptimizedSkeletalMeshInstanceHandle InHandle, bool bInPlaying);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool SetInstanceVisible(FOptimizedSkeletalMeshInstanceHandle InHandle, bool bInVisible);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool ShowInstance(FOptimizedSkeletalMeshInstanceHandle InHandle);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool HideInstance(FOptimizedSkeletalMeshInstanceHandle InHandle);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	int32 SetInstancesVisible(const TArray<FOptimizedSkeletalMeshInstanceHandle>& InHandles, bool bInVisible);

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	bool GetInstance(FOptimizedSkeletalMeshInstanceHandle InHandle, FOptimizedSkeletalMeshInstanceDesc& OutDesc) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	void GetInstancesSnapshot(TArray<FOptimizedSkeletalMeshInstanceSnapshot>& OutInstances) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	int32 GetInstanceCount() const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	int32 GetVisibleRenderBatchCount() const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	FOptimizedSkeletalMeshAnimationStats GetLastAnimationStats() const;

	const TArray<FMatrix44f>* GetInstanceBonePalette(FOptimizedSkeletalMeshInstanceHandle InHandle) const;
	void GetBonePaletteSnapshots(TArray<FOptimizedSkeletalMeshBonePaletteSnapshot>& OutSnapshots) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Animation")
	int32 GetCachedBonePaletteCount() const;

	bool HasDirtyBonePalettes() const
	{
		return bBonePalettesDirty;
	}

	void ClearDirtyBonePalettes()
	{
		bBonePalettesDirty = false;
		DirtyBonePaletteInstanceIds.Reset();
	}

	bool IsRenderDataDirty() const
	{
		return bRenderDataDirty;
	}

	void SetExternalRenderBridgeActive(bool bInActive);

	void ClearRenderDataDirty()
	{
		bRenderDataDirty = false;
	}

private:
#pragma region Rendering
	void EnsureRenderBridge();
	void DestroyRenderBridge();
#pragma endregion

#pragma region Instances
	int32 AllocateInstanceId();
	bool IsValidInstanceId(int32 InInstanceId) const;
	void MarkRenderDataDirty();
	void RefreshAnimationTracking(int32 InInstanceId, const FOptimizedSkeletalMeshInstanceDesc& InDesc, bool bInForceDirty);
	void RemoveAnimationTracking(int32 InInstanceId);
	static bool ShouldTickAnimation(const FOptimizedSkeletalMeshInstanceDesc& InDesc);
#pragma endregion

#pragma region Animation
	void TickAnimation(float InDeltaTime);
	FOptimizedSkeletalMeshAnimationMeshCache* FindOrBuildAnimationMeshCache(USkeletalMesh* InSkeletalMesh);
	bool EvaluateInstanceBonePalette(const FOptimizedSkeletalMeshInstanceDesc& InDesc, TArray<FMatrix44f>& OutBonePalette);
	static bool EvaluateInstanceBonePaletteWithCache(
		const FOptimizedSkeletalMeshInstanceDesc& InDesc,
		const FOptimizedSkeletalMeshAnimationMeshCache& InMeshCache,
		TArray<FMatrix44f>& OutBonePalette);
	static float WrapAnimationTime(float InAnimationTime, float InSequenceLength);
#pragma endregion

#pragma region State
	UPROPERTY(Transient)
	TMap<int32, FOptimizedSkeletalMeshInstanceDesc> Instances;

	UPROPERTY(Transient)
	TArray<int32> FreeInstanceIds;

	UPROPERTY(Transient)
	TObjectPtr<AActor> RenderBridgeActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UOptimizedSkeletalMeshRenderComponent> RenderComponent = nullptr;

	TMap<TObjectKey<USkeletalMesh>, FOptimizedSkeletalMeshAnimationMeshCache> AnimationMeshCaches;
	TMap<int32, TArray<FMatrix44f>> InstanceBonePalettes;
	TSet<int32> ActiveAnimationInstanceIds;
	TSet<int32> DirtyAnimationInstanceIds;
	TSet<int32> DirtyBonePaletteInstanceIds;
	TMap<int32, float> AnimationUpdateAccumulators;

	int32 NextInstanceId = 1;
	bool bRenderDataDirty = false;
	bool bBonePalettesDirty = false;
	bool bExternalRenderBridgeActive = false;
	FOptimizedSkeletalMeshAnimationStats LastAnimationStats;
#pragma endregion
};
