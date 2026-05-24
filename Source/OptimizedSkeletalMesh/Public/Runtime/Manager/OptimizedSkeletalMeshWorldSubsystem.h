// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoneContainer.h"
#include "CoreMinimal.h"
#include "Runtime/Rendering/OptimizedSkeletalMeshRenderComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "OptimizedSkeletalMeshWorldSubsystem.generated.h"

class USkeletalMesh;
class UAnimSequence;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class AActor;
class AOptimizedSkeletalMeshRenderBridgeActor;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	bool bCastLocalLightShadows = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	bool bRenderCustomDepth = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0", ClampMax = "255"))
	int32 CustomDepthStencilValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	TObjectPtr<UMaterialInterface> MaterialOverride = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	FVector4f MaterialCustomData0 = FVector4f::Zero();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	FVector4f MaterialCustomData1 = FVector4f::Zero();
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
	int32 DistanceRateScaledInstances = 0;

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
	int32 RenderVisibleAnimatedInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	int32 DirtyCpuPaletteInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	int32 DirtyGpuPaletteInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	int32 GpuPaletteUploadSkippedInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	int32 TotalBoneMatrices = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	int32 MaxBonesPerInstance = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	float MinEffectiveUpdateRateHz = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	float MaxEffectiveUpdateRateHz = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Animation")
	float AverageEffectiveUpdateRateHz = 0.0f;

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

struct FOptimizedSkeletalMeshAnimationEvaluationWork
{
	int32 InstanceId = INDEX_NONE;
	FOptimizedSkeletalMeshInstanceDesc Desc;
	const FOptimizedSkeletalMeshAnimationMeshCache* MeshCache = nullptr;
};

struct FOptimizedSkeletalMeshAnimationEvaluationResult
{
	int32 InstanceId = INDEX_NONE;
	TArray<FMatrix44f> BonePalette;
	bool bSucceeded = false;
};

struct FOptimizedSkeletalMeshBonePaletteSnapshot
{
	FOptimizedSkeletalMeshInstanceHandle Handle;
	TArray<FMatrix44f> PreviousBonePalette;
	TArray<FMatrix44f> BonePalette;
	float BlendAlpha = 1.0f;
};

USTRUCT(BlueprintType)
struct OPTIMIZEDSKELETALMESH_API FOptimizedSkeletalMeshRenderSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	bool bDrawDebugBounds = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	bool bDrawMeshSections = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	EOptimizedSkeletalMeshDrawMode MeshDrawMode = EOptimizedSkeletalMeshDrawMode::GpuSkinnedInstanced;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	bool bEnableInstanceFrustumCulling = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "1.0"))
	float InstanceCullBoundsScale = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	bool bUseConservativeProxyBounds = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "1000.0"))
	float ConservativeProxyBoundsExtent = 10000000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	bool bDrawCullingDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	bool bDrawCullTestBounds = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	bool bCastShadows = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0.0"))
	float NearFullShadowDistance = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0.0"))
	float MidShadowDistance = 3200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "1"))
	int32 MidShadowUpdateDivisor = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0"))
	int32 FarShadowUpdateDivisor = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0.0"))
	float MaxShadowCastDistance = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0"))
	int32 MaxDynamicShadowCasters = 120;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0"))
	int32 NearShadowLodBias = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0"))
	int32 MidShadowLodBias = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0"))
	int32 FarShadowLodBias = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0"))
	int32 MaxShadowSectionsPerLOD = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0.0"))
	float LocalLightMaxShadowCastDistance = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0"))
	int32 LocalLightMaxDynamicShadowCasters = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0"))
	int32 LocalLightShadowLodBias = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0"))
	int32 LocalLightMaxShadowSectionsPerLOD = 1;
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
	FOptimizedSkeletalMeshInstanceHandle AddInstance(const FOptimizedSkeletalMeshInstanceDesc& InDesc);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	void AddInstancesBatch(
		const FOptimizedSkeletalMeshInstanceDesc& InBaseDesc,
		const TArray<FTransform>& InWorldTransforms,
		TArray<FOptimizedSkeletalMeshInstanceHandle>& OutHandles);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool RemoveInstance(FOptimizedSkeletalMeshInstanceHandle InHandle);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool RemoveInstanceById(int32 InInstanceId);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	int32 RemoveInstances(const TArray<FOptimizedSkeletalMeshInstanceHandle>& InHandles);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	int32 RemoveInstancesById(const TArray<int32>& InInstanceIds);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool UpdateInstance(FOptimizedSkeletalMeshInstanceHandle InHandle, const FOptimizedSkeletalMeshInstanceDesc& InDesc);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool UpdateInstanceTransform(FOptimizedSkeletalMeshInstanceHandle InHandle, const FTransform& InWorldTransform);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool UpdateInstanceTransformById(int32 InInstanceId, const FTransform& InWorldTransform);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	int32 UpdateInstancesTransform(
		const TArray<FOptimizedSkeletalMeshInstanceHandle>& InHandles,
		const TArray<FTransform>& InWorldTransforms);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	int32 UpdateInstancesTransformById(const TArray<int32>& InInstanceIds, const TArray<FTransform>& InWorldTransforms);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool SetInstanceLocation(FOptimizedSkeletalMeshInstanceHandle InHandle, const FVector& InWorldLocation);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool SetInstanceLocationById(int32 InInstanceId, const FVector& InWorldLocation);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool SetInstanceRotation(FOptimizedSkeletalMeshInstanceHandle InHandle, const FRotator& InWorldRotation);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool SetInstanceRotationById(int32 InInstanceId, const FRotator& InWorldRotation);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool SetInstanceScale(FOptimizedSkeletalMeshInstanceHandle InHandle, const FVector& InWorldScale3D);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool SetInstanceScaleById(int32 InInstanceId, const FVector& InWorldScale3D);

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	bool GetInstanceTransform(FOptimizedSkeletalMeshInstanceHandle InHandle, FTransform& OutWorldTransform) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	bool GetInstanceTransformById(int32 InInstanceId, FTransform& OutWorldTransform) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	bool GetInstanceLocation(FOptimizedSkeletalMeshInstanceHandle InHandle, FVector& OutWorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	bool GetInstanceLocationById(int32 InInstanceId, FVector& OutWorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	bool GetInstanceRotation(FOptimizedSkeletalMeshInstanceHandle InHandle, FRotator& OutWorldRotation) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	bool GetInstanceRotationById(int32 InInstanceId, FRotator& OutWorldRotation) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	bool GetInstanceScale(FOptimizedSkeletalMeshInstanceHandle InHandle, FVector& OutWorldScale3D) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	bool GetInstanceScaleById(int32 InInstanceId, FVector& OutWorldScale3D) const;

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool UpdateInstanceAnimationTime(FOptimizedSkeletalMeshInstanceHandle InHandle, float InAnimationTime);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool SetInstanceAnimationPlaying(FOptimizedSkeletalMeshInstanceHandle InHandle, bool bInPlaying);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool SetInstanceAnimationUpdateRateHz(FOptimizedSkeletalMeshInstanceHandle InHandle, float InAnimationUpdateRateHz);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool SetInstanceVisible(FOptimizedSkeletalMeshInstanceHandle InHandle, bool bInVisible);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool ShowInstance(FOptimizedSkeletalMeshInstanceHandle InHandle);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	bool HideInstance(FOptimizedSkeletalMeshInstanceHandle InHandle);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh")
	int32 SetInstancesVisible(const TArray<FOptimizedSkeletalMeshInstanceHandle>& InHandles, bool bInVisible);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceCustomDepth(
		FOptimizedSkeletalMeshInstanceHandle InHandle,
		bool bInRenderCustomDepth,
		int32 InCustomDepthStencilValue = 0);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceCustomDepthById(
		int32 InInstanceId,
		bool bInRenderCustomDepth,
		int32 InCustomDepthStencilValue = 0);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceRenderCustomDepth(FOptimizedSkeletalMeshInstanceHandle InHandle, bool bInRenderCustomDepth);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceRenderCustomDepthById(int32 InInstanceId, bool bInRenderCustomDepth);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceCustomDepthStencilValue(
		FOptimizedSkeletalMeshInstanceHandle InHandle,
		int32 InCustomDepthStencilValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceCustomDepthStencilValueById(int32 InInstanceId, int32 InCustomDepthStencilValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterial(FOptimizedSkeletalMeshInstanceHandle InHandle, UMaterialInterface* InMaterial);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterialById(int32 InInstanceId, UMaterialInterface* InMaterial);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterialScalarParam(FOptimizedSkeletalMeshInstanceHandle InHandle, int32 InParamIndex, float InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterialScalarParamById(int32 InInstanceId, int32 InParamIndex, float InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterialVectorParam(FOptimizedSkeletalMeshInstanceHandle InHandle, int32 InStartParamIndex, const FVector& InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterialVectorParamById(int32 InInstanceId, int32 InStartParamIndex, const FVector& InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterialBoolParam(FOptimizedSkeletalMeshInstanceHandle InHandle, int32 InParamIndex, bool bInValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterialBoolParamById(int32 InInstanceId, int32 InParamIndex, bool bInValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterialTextureParam(
		FOptimizedSkeletalMeshInstanceHandle InHandle,
		FName InParameterName,
		UTexture2D* InTexture);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterialTextureParamById(int32 InInstanceId, FName InParameterName, UTexture2D* InTexture);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterialScalarParamByName(FOptimizedSkeletalMeshInstanceHandle InHandle, FName InParameterName, float InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterialScalarParamByNameId(int32 InInstanceId, FName InParameterName, float InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterialVectorParamByName(
		FOptimizedSkeletalMeshInstanceHandle InHandle,
		FName InParameterName,
		const FVector& InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterialVectorParamByNameId(int32 InInstanceId, FName InParameterName, const FVector& InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterialBoolParamByName(FOptimizedSkeletalMeshInstanceHandle InHandle, FName InParameterName, bool bInValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterialBoolParamByNameId(int32 InInstanceId, FName InParameterName, bool bInValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterialColorParam(
		FOptimizedSkeletalMeshInstanceHandle InHandle,
		int32 InStartParamIndex,
		const FLinearColor& InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterialColorParamById(int32 InInstanceId, int32 InStartParamIndex, const FLinearColor& InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterialColorParamByName(
		FOptimizedSkeletalMeshInstanceHandle InHandle,
		FName InParameterName,
		const FLinearColor& InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	bool SetInstanceMaterialColorParamByNameId(int32 InInstanceId, FName InParameterName, const FLinearColor& InValue);

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	bool GetInstance(FOptimizedSkeletalMeshInstanceHandle InHandle, FOptimizedSkeletalMeshInstanceDesc& OutDesc) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	bool GetInstanceById(int32 InInstanceId, FOptimizedSkeletalMeshInstanceDesc& OutDesc) const;

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Animation")
	bool SetInstanceAnimationAsset(
		FOptimizedSkeletalMeshInstanceHandle InHandle,
		UAnimSequence* InAnimation,
		bool bInResetTime = true);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Animation")
	bool SetInstanceAnimationAssetById(int32 InInstanceId, UAnimSequence* InAnimation, bool bInResetTime = true);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Animation")
	bool PlayInstanceAnimation(FOptimizedSkeletalMeshInstanceHandle InHandle);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Animation")
	bool PlayInstanceAnimationById(int32 InInstanceId);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Animation")
	bool PauseInstanceAnimation(FOptimizedSkeletalMeshInstanceHandle InHandle);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Animation")
	bool PauseInstanceAnimationById(int32 InInstanceId);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Animation")
	bool StopInstanceAnimation(FOptimizedSkeletalMeshInstanceHandle InHandle, bool bInResetTime = true);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Animation")
	bool StopInstanceAnimationById(int32 InInstanceId, bool bInResetTime = true);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Animation")
	bool SetInstanceAnimationLooping(FOptimizedSkeletalMeshInstanceHandle InHandle, bool bInLoopAnimation);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Animation")
	bool SetInstanceAnimationLoopingById(int32 InInstanceId, bool bInLoopAnimation);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Animation")
	bool SetInstanceAnimationPlayRate(FOptimizedSkeletalMeshInstanceHandle InHandle, float InAnimationPlayRate);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Animation")
	bool SetInstanceAnimationPlayRateById(int32 InInstanceId, float InAnimationPlayRate);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Animation")
	bool SetInstanceAnimationTime(FOptimizedSkeletalMeshInstanceHandle InHandle, float InAnimationTime);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Animation")
	bool SetInstanceAnimationTimeById(int32 InInstanceId, float InAnimationTime);

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	void GetInstancesSnapshot(TArray<FOptimizedSkeletalMeshInstanceSnapshot>& OutInstances) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	int32 GetInstanceCount() const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	int32 GetVisibleRenderBatchCount() const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh")
	FOptimizedSkeletalMeshAnimationStats GetLastAnimationStats() const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Rendering")
	FOptimizedSkeletalMeshRenderStats GetLastRenderStats() const;

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	void ApplyRenderSettings(const FOptimizedSkeletalMeshRenderSettings& InSettings);

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Rendering")
	FOptimizedSkeletalMeshRenderSettings GetRenderSettings() const;

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	void ReloadRenderSettingsFromCVars();

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	void RegisterExternalRenderComponent(UOptimizedSkeletalMeshRenderComponent* InComponent);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Rendering")
	void UnregisterExternalRenderComponent(UOptimizedSkeletalMeshRenderComponent* InComponent);

	void ApplyRenderSettingsToComponent(UOptimizedSkeletalMeshRenderComponent* InComponent) const;

	const TArray<FMatrix44f>* GetInstanceBonePalette(FOptimizedSkeletalMeshInstanceHandle InHandle) const;
	float GetInstanceAnimationBlendAlpha(FOptimizedSkeletalMeshInstanceHandle InHandle) const;
	void GetBonePaletteSnapshots(TArray<FOptimizedSkeletalMeshBonePaletteSnapshot>& OutSnapshots) const;
	void UpdateRenderVisibleInstanceIds(TConstArrayView<int32> InVisibleInstanceIds);
	void UpdateLastRenderStats(const FOptimizedSkeletalMeshRenderStats& InStats);

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Animation")
	int32 GetCachedBonePaletteCount() const;

	bool HasDirtyBonePalettes() const
	{
		return HasDirtyRenderVisibleBonePalettes();
	}

	void ClearDirtyBonePalettes()
	{
		ClearDirtyRenderVisibleBonePalettes();
	}

	bool IsRenderDataDirty() const
	{
		return bRenderDataDirty || bCustomDepthRenderDataDirty;
	}

	void SetExternalRenderBridgeActive(bool bInActive);

	void ClearRenderDataDirty()
	{
		bRenderDataDirty = false;
		bCustomDepthRenderDataDirty = false;
	}

private:
#pragma region Rendering
	void EnsureRenderBridge();
	void DestroyRenderBridge();
	void ApplyRenderSettingsToComponent();
	void RefreshActiveRenderSettings(bool bInForce);
	void RefreshCustomDepthRenderComponents();
	void RequestRenderRefreshForAllComponents();
	void RequestCustomDepthRenderRefresh();
	bool PushBonePalettesToRenderComponents();
	bool PushInstanceTransformsToRenderComponents();
#pragma endregion

#pragma region Instances
	int32 AllocateInstanceId();
	bool IsValidInstanceId(int32 InInstanceId) const;
	void MarkRenderDataDirty();
	void MarkCustomDepthRenderDataDirty();
	void MarkTransformDirty(int32 InInstanceId);
	bool HasDirtyTransforms() const;
	void ClearDirtyTransforms();
	void RefreshAnimationTracking(int32 InInstanceId, const FOptimizedSkeletalMeshInstanceDesc& InDesc, bool bInForceDirty);
	void RemoveAnimationTracking(int32 InInstanceId);
	void MarkBonePaletteDirty(int32 InInstanceId);
	bool HasDirtyRenderVisibleBonePalettes() const;
	void ClearDirtyRenderVisibleBonePalettes();
	bool ResolveNamedMaterialParamSlot(FName InParameterName, int32 InWidth, int32& OutStartIndex);
	float GetEffectiveAnimationUpdateRateHz(const FOptimizedSkeletalMeshInstanceDesc& InDesc, float InNearestCameraDistance) const;
	static float GetUpdateRateScaleForDistance(float InDistance);
	bool GetNearestCameraDistance(const FVector& InWorldLocation, float& OutDistance) const;
	static bool ShouldTickAnimation(const FOptimizedSkeletalMeshInstanceDesc& InDesc);
#pragma endregion

#pragma region Animation
	void TickAnimation(float InDeltaTime);
	void InitializeAnimationStats(FOptimizedSkeletalMeshAnimationStats& OutStats, float InDeltaTime) const;
	void FinalizeAnimationStats(FOptimizedSkeletalMeshAnimationStats& OutStats) const;
	void BuildAnimationInstanceIdsToProcess(TArray<int32>& OutInstanceIdsToProcess, TArray<int32>& OutDirtyInstanceIdsToProcess) const;
	void BuildAnimationEvaluationWork(
		float InDeltaTime,
		TConstArrayView<int32> InInstanceIdsToProcess,
		FOptimizedSkeletalMeshAnimationStats& OutStats,
		TArray<FOptimizedSkeletalMeshAnimationEvaluationWork>& OutEvaluationWork);
	bool AdvanceAnimationTime(
		int32 InInstanceId,
		FOptimizedSkeletalMeshInstanceDesc& InOutDesc,
		float InAnimationDeltaTime,
		FOptimizedSkeletalMeshAnimationStats& OutStats);
	void RemoveInstanceAnimationData(int32 InInstanceId);
	void RunAnimationEvaluationWork(
		const TArray<FOptimizedSkeletalMeshAnimationEvaluationWork>& InEvaluationWork,
		FOptimizedSkeletalMeshAnimationStats& OutStats,
		TArray<FOptimizedSkeletalMeshAnimationEvaluationResult>& OutEvaluationResults) const;
	void ApplyAnimationEvaluationResults(
		TArray<FOptimizedSkeletalMeshAnimationEvaluationResult>& InOutEvaluationResults,
		FOptimizedSkeletalMeshAnimationStats& OutStats);
	void ClearProcessedDirtyAnimationIds(TConstArrayView<int32> InProcessedDirtyInstanceIds);
	FOptimizedSkeletalMeshAnimationMeshCache* FindOrBuildAnimationMeshCache(USkeletalMesh* InSkeletalMesh);
	bool EvaluateInstanceBonePalette(const FOptimizedSkeletalMeshInstanceDesc& InDesc, TArray<FMatrix44f>& OutBonePalette);
	static bool EvaluateInstanceBonePaletteWithCache(
		const FOptimizedSkeletalMeshInstanceDesc& InDesc,
		const FOptimizedSkeletalMeshAnimationMeshCache& InMeshCache,
		TArray<FMatrix44f>& OutBonePalette);
	static float WrapAnimationTime(float InAnimationTime, float InSequenceLength);
#pragma endregion

#pragma region Debug
	void DrawInstanceDebugOverlay() const;
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

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UOptimizedSkeletalMeshRenderComponent>> CustomDepthRenderComponents;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> MaterialTextureOverrideCache;

	UPROPERTY(Transient)
	TMap<FName, FIntPoint> NamedMaterialParamSlots;

	int32 NextNamedMaterialParamSlot = 0;

	TMap<TObjectKey<USkeletalMesh>, FOptimizedSkeletalMeshAnimationMeshCache> AnimationMeshCaches;
	TMap<int32, TArray<FMatrix44f>> PreviousInstanceBonePalettes;
	TMap<int32, TArray<FMatrix44f>> InstanceBonePalettes;
	TMap<int32, float> InstanceAnimationBlendAlphas;
	TSet<int32> ActiveAnimationInstanceIds;
	TSet<int32> DirtyAnimationInstanceIds;
	TSet<int32> DirtyBonePaletteInstanceIds;
	TSet<int32> DirtyTransformInstanceIds;
	TSet<int32> RenderVisibleInstanceIds;
	TMap<int32, float> AnimationUpdateAccumulators;

	int32 NextInstanceId = 1;
	int32 BulkUpdateDepth = 0;
	int32 LastSeenRenderCVarVersion = 0;
	int32 RenderStateRecoveryAttempts = 0;
	mutable int32 CachedVisibleRenderBatchCount = 0;
	bool bRenderDataDirty = false;
	bool bCustomDepthRenderDataDirty = false;
	mutable bool bVisibleRenderBatchCountDirty = true;
	bool bExternalRenderBridgeActive = false;
	FOptimizedSkeletalMeshAnimationStats LastAnimationStats;
	FOptimizedSkeletalMeshRenderStats LastRenderStats;
	FOptimizedSkeletalMeshRenderSettings CurrentRenderSettings;
	FOptimizedSkeletalMeshRenderSettings ActiveRenderSettings;
	TArray<TWeakObjectPtr<UOptimizedSkeletalMeshRenderComponent>> ExternalRenderComponents;
#pragma endregion
};
