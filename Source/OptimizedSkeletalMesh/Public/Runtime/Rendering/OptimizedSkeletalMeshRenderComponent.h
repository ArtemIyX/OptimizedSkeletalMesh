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
	DirectMeshInstanced UMETA(DisplayName = "Direct Mesh Instanced")
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
	TArray<int32> VisibleInstancesByLOD;
};

UCLASS(NotBlueprintable, ClassGroup = Rendering)
class OPTIMIZEDSKELETALMESH_API UOptimizedSkeletalMeshRenderComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UOptimizedSkeletalMeshRenderComponent(const FObjectInitializer& ObjectInitializer);

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
	void RequestRenderRefresh();
	void PushBonePalettesToRenderThread();

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
	const FOptimizedSkeletalMeshRenderStats& GetLastRenderStats() const { return LastRenderStats; }
	void ApplyRenderStats_GameThread(const FOptimizedSkeletalMeshRenderStats& InStats);

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual bool ShouldRecreateProxyOnUpdateTransform() const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;

private:
	UPROPERTY(EditAnywhere, Category = "Optimized Skeletal Mesh|Debug")
	bool bDrawDebugBounds = true;

	UPROPERTY(EditAnywhere, Category = "Optimized Skeletal Mesh|Debug")
	bool bDrawMeshSections = false;

	UPROPERTY(EditAnywhere, Category = "Optimized Skeletal Mesh|Debug")
	EOptimizedSkeletalMeshDrawMode MeshDrawMode = EOptimizedSkeletalMeshDrawMode::DynamicMeshProof;

	UPROPERTY(EditAnywhere, Category = "Optimized Skeletal Mesh|Debug", meta = (ClampMin = "0"))
	int32 MaxMeshDrawInstances = 1;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh|Stats", meta = (AllowPrivateAccess = "true"))
	FOptimizedSkeletalMeshRenderStats LastRenderStats;

	UPROPERTY(Transient)
	TObjectPtr<UOptimizedSkeletalMeshWorldSubsystem> Subsystem = nullptr;
};
