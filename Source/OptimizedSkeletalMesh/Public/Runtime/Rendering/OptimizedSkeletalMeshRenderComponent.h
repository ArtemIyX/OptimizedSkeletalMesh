// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "OptimizedSkeletalMeshRenderComponent.generated.h"

class FPrimitiveSceneProxy;
class UOptimizedSkeletalMeshWorldSubsystem;

UENUM(BlueprintType)
enum class EOptimizedSkeletalMeshDrawMode : uint8
{
	DynamicMeshProof UMETA(DisplayName = "Dynamic Mesh Proof"),
	DirectMeshPerInstance UMETA(DisplayName = "Direct Mesh Per Instance"),
	DirectMeshInstanced UMETA(DisplayName = "Direct Mesh Instanced"),
	GpuSkinnedInstanced UMETA(DisplayName = "GPU Skinned Instanced")
};

USTRUCT(BlueprintType)
struct OPTIMIZEDSKELETALMESH_API FOptimizedSkeletalMeshRenderStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 FrameNumber = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 RegisteredInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 MeshBatches = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 TestedInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 VisibleInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 ShadowVisibleInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 CulledInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 DrawnInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SubmittedDrawCalls = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SubmittedSections = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SubmittedTriangles = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SkinningPaletteInstances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SkinningPaletteMatrices = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SkinningPaletteBytes = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SkinningGPUPaletteMatrices = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SkinningGPUPaletteBytes = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SkinningGPUPaletteUploads = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SkinningSkinWeightLODs = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SkinningSkinWeightVertices = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SkinningMaxBoneInfluences = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SkinningSkinWeight16BitIndexLODs = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SkinningSkinWeight16BitWeightLODs = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SkinningMissingSkinWeightLODs = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SkinningGPUSkinReadyLODs = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SkinningGPUSkinFallbackDraws = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SkinningGPUSkinDraws = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SkinningGPUInstanceOffsetEntries = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	int32 SkinningGPUSectionBoneMapEntries = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	TArray<int32> VisibleInstancesByLOD;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats")
	TArray<int32> RenderVisibleInstanceIds;
};

UCLASS(NotBlueprintable, ClassGroup = Rendering)
class OPTIMIZEDSKELETALMESH_API UOptimizedSkeletalMeshRenderComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
#pragma region Setup
	UOptimizedSkeletalMeshRenderComponent(const FObjectInitializer& InObjectInitializer);

	void SetOptimizedSkeletalMeshSubsystem(UOptimizedSkeletalMeshWorldSubsystem* InSubsystem);
	void SetDrawDebugBounds(bool bInDrawDebugBounds);
	void SetDrawMeshSections(bool bInDrawMeshSections);
	void SetMeshDrawMode(EOptimizedSkeletalMeshDrawMode InMeshDrawMode);
	void SetMaxMeshDrawInstances(int32 InMaxMeshDrawInstances);
	void SetInstanceFrustumCulling(bool bInEnableInstanceFrustumCulling);
	void SetInstanceCullBoundsScale(float InInstanceCullBoundsScale);
	void SetConservativeProxyBounds(bool bInUseConservativeProxyBounds);
	void SetConservativeProxyBoundsExtent(float InConservativeProxyBoundsExtent);
	void SetDrawCullingDebug(bool bInDrawCullingDebug);
	void SetDrawCullTestBounds(bool bInDrawCullTestBounds);
	void SetCastShadows(bool bInCastShadows);
	void SetNearFullShadowDistance(float InNearFullShadowDistance);
	void SetMidShadowDistance(float InMidShadowDistance);
	void SetMidShadowUpdateDivisor(int32 InMidShadowUpdateDivisor);
	void SetFarShadowUpdateDivisor(int32 InFarShadowUpdateDivisor);
	void SetMaxShadowCastDistance(float InMaxShadowCastDistance);
	void SetMaxDynamicShadowCasters(int32 InMaxDynamicShadowCasters);
	void SetNearShadowLodBias(int32 InNearShadowLodBias);
	void SetMidShadowLodBias(int32 InMidShadowLodBias);
	void SetFarShadowLodBias(int32 InFarShadowLodBias);
	void SetMaxShadowSectionsPerLOD(int32 InMaxShadowSectionsPerLOD);
	void RequestRenderRefresh();
	bool PushBonePalettesToRenderThread();
#pragma endregion

#pragma region Accessors
	bool ShouldDrawDebugBounds() const { return bDrawDebugBounds; }
	bool ShouldDrawMeshSections() const { return bDrawMeshSections; }
	EOptimizedSkeletalMeshDrawMode GetMeshDrawMode() const { return MeshDrawMode; }
	int32 GetMaxMeshDrawInstances() const { return MaxMeshDrawInstances; }
	bool ShouldEnableInstanceFrustumCulling() const { return bEnableInstanceFrustumCulling; }
	float GetInstanceCullBoundsScale() const { return InstanceCullBoundsScale; }
	bool ShouldUseConservativeProxyBounds() const { return bUseConservativeProxyBounds; }
	float GetConservativeProxyBoundsExtent() const { return ConservativeProxyBoundsExtent; }
	bool ShouldDrawCullingDebug() const { return bDrawCullingDebug; }
	bool ShouldDrawCullTestBounds() const { return bDrawCullTestBounds; }
	bool ShouldCastShadows() const { return bCastShadows; }
	float GetNearFullShadowDistance() const { return NearFullShadowDistance; }
	float GetMidShadowDistance() const { return MidShadowDistance; }
	int32 GetMidShadowUpdateDivisor() const { return MidShadowUpdateDivisor; }
	int32 GetFarShadowUpdateDivisor() const { return FarShadowUpdateDivisor; }
	float GetMaxShadowCastDistance() const { return MaxShadowCastDistance; }
	int32 GetMaxDynamicShadowCasters() const { return MaxDynamicShadowCasters; }
	int32 GetNearShadowLodBias() const { return NearShadowLodBias; }
	int32 GetMidShadowLodBias() const { return MidShadowLodBias; }
	int32 GetFarShadowLodBias() const { return FarShadowLodBias; }
	int32 GetMaxShadowSectionsPerLOD() const { return MaxShadowSectionsPerLOD; }
	const FOptimizedSkeletalMeshRenderStats& GetLastRenderStats() const { return LastRenderStats; }
	void ApplyRenderStats_GameThread(const FOptimizedSkeletalMeshRenderStats& InStats);
#pragma endregion

#pragma region UPrimitiveComponent
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& InLocalToWorld) const override;
	virtual bool ShouldRecreateProxyOnUpdateTransform() const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
#pragma endregion

private:
#pragma region Settings
	UPROPERTY(EditAnywhere, Category = "Optimized Skeletal Mesh|Debug")
	bool bDrawDebugBounds = true;

	UPROPERTY(EditAnywhere, Category = "Optimized Skeletal Mesh|Debug")
	bool bDrawMeshSections = false;

	UPROPERTY(EditAnywhere, Category = "Optimized Skeletal Mesh|Debug")
	EOptimizedSkeletalMeshDrawMode MeshDrawMode = EOptimizedSkeletalMeshDrawMode::DynamicMeshProof;

	UPROPERTY(EditAnywhere, Category = "Optimized Skeletal Mesh|Debug", meta = (ClampMin = "0"))
	int32 MaxMeshDrawInstances = 0;

	UPROPERTY(Transient)
	bool bEnableInstanceFrustumCulling = true;

	UPROPERTY(Transient)
	float InstanceCullBoundsScale = 1.5f;

	UPROPERTY(Transient)
	bool bUseConservativeProxyBounds = true;

	UPROPERTY(Transient)
	float ConservativeProxyBoundsExtent = 10000000.0f;

	UPROPERTY(Transient)
	bool bDrawCullingDebug = false;

	UPROPERTY(Transient)
	bool bDrawCullTestBounds = true;

	UPROPERTY(Transient)
	bool bCastShadows = true;

	UPROPERTY(Transient)
	float NearFullShadowDistance = 1800.0f;

	UPROPERTY(Transient)
	float MidShadowDistance = 3200.0f;

	UPROPERTY(Transient)
	int32 MidShadowUpdateDivisor = 2;

	UPROPERTY(Transient)
	int32 FarShadowUpdateDivisor = 0;

	UPROPERTY(Transient)
	float MaxShadowCastDistance = 5000.0f;

	UPROPERTY(Transient)
	int32 MaxDynamicShadowCasters = 120;

	UPROPERTY(Transient)
	int32 NearShadowLodBias = 0;

	UPROPERTY(Transient)
	int32 MidShadowLodBias = 1;

	UPROPERTY(Transient)
	int32 FarShadowLodBias = 2;

	UPROPERTY(Transient)
	int32 MaxShadowSectionsPerLOD = 2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats", meta = (AllowPrivateAccess = "true"))
	FOptimizedSkeletalMeshRenderStats LastRenderStats;
#pragma endregion

#pragma region State
	UPROPERTY(Transient)
	TObjectPtr<UOptimizedSkeletalMeshWorldSubsystem> Subsystem = nullptr;
#pragma endregion
};
