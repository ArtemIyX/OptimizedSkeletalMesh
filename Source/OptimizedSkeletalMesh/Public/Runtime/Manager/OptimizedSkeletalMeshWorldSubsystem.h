// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OptimizedSkeletalMeshWorldSubsystem.generated.h"

class USkeletalMesh;
class AActor;
class UOptimizedSkeletalMeshRenderComponent;

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
	float AnimationTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh")
	bool bVisible = true;
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
	bool SetInstanceVisible(FOptimizedSkeletalMeshInstanceHandle Handle, bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	bool GetInstance(FOptimizedSkeletalMeshInstanceHandle Handle, FOptimizedSkeletalMeshInstanceDesc& OutDesc) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	void GetInstancesSnapshot(TArray<FOptimizedSkeletalMeshInstanceSnapshot>& OutInstances) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	int32 GetInstanceCount() const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	int32 GetVisibleRenderBatchCount() const;

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

	UPROPERTY(Transient)
	TMap<int32, FOptimizedSkeletalMeshInstanceDesc> Instances;

	UPROPERTY(Transient)
	TArray<int32> FreeInstanceIds;

	UPROPERTY(Transient)
	TObjectPtr<AActor> RenderBridgeActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UOptimizedSkeletalMeshRenderComponent> RenderComponent = nullptr;

	int32 NextInstanceId = 1;
	bool bRenderDataDirty = false;
	bool bExternalRenderBridgeActive = false;
};
