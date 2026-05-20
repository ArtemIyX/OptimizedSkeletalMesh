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

UCLASS()
class OPTIMIZEDSKELETALMESH_API UOptimizedSkeletalMeshWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	FOptimizedSkeletalMeshInstanceHandle RegisterInstance(const FOptimizedSkeletalMeshInstanceDesc& Desc);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool UnregisterInstance(FOptimizedSkeletalMeshInstanceHandle Handle);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool UpdateInstance(FOptimizedSkeletalMeshInstanceHandle Handle, const FOptimizedSkeletalMeshInstanceDesc& Desc);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool UpdateInstanceTransform(FOptimizedSkeletalMeshInstanceHandle Handle, const FTransform& WorldTransform);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool UpdateInstanceAnimationTime(FOptimizedSkeletalMeshInstanceHandle Handle, float AnimationTime);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool SetInstanceAnimationPlaying(FOptimizedSkeletalMeshInstanceHandle Handle, bool bPlaying);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool SetInstanceVisible(FOptimizedSkeletalMeshInstanceHandle Handle, bool bVisible);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool ShowInstance(FOptimizedSkeletalMeshInstanceHandle Handle);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool HideInstance(FOptimizedSkeletalMeshInstanceHandle Handle);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	int32 SetInstancesVisible(const TArray<FOptimizedSkeletalMeshInstanceHandle>& Handles, bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	bool GetInstance(FOptimizedSkeletalMeshInstanceHandle Handle, FOptimizedSkeletalMeshInstanceDesc& OutDesc) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	void GetInstancesSnapshot(TArray<FOptimizedSkeletalMeshInstanceSnapshot>& OutInstances) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	int32 GetInstanceCount() const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	int32 GetVisibleRenderBatchCount() const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	FOptimizedSkeletalMeshAnimationStats GetLastAnimationStats() const;

	const TArray<FMatrix44f>* GetInstanceBonePalette(FOptimizedSkeletalMeshInstanceHandle Handle) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Animation")
	int32 GetCachedBonePaletteCount() const;

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
	void EnsureRenderBridge();
	void DestroyRenderBridge();
	int32 AllocateInstanceId();
	bool IsValidInstanceId(int32 InstanceId) const;
	void MarkRenderDataDirty();
	void TickAnimation(float DeltaTime);
	FOptimizedSkeletalMeshAnimationMeshCache* FindOrBuildAnimationMeshCache(USkeletalMesh* SkeletalMesh);
	bool EvaluateInstanceBonePalette(const FOptimizedSkeletalMeshInstanceDesc& Desc, TArray<FMatrix44f>& OutBonePalette);
	static float WrapAnimationTime(float AnimationTime, float SequenceLength);

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

	int32 NextInstanceId = 1;
	bool bRenderDataDirty = false;
	bool bExternalRenderBridgeActive = false;
	FOptimizedSkeletalMeshAnimationStats LastAnimationStats;
};
