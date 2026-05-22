// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Engine/DeveloperSettings.h"
#include "Runtime/Rendering/OptimizedSkeletalMeshRenderComponent.h"
#include "OptimizedSkeletalMeshSettings.generated.h"

UENUM(BlueprintType)
enum class EOptimizedSkeletalMeshDistanceBasedRateMode : uint8
{
	Static UMETA(DisplayName = "Static"),
	DistanceBasedArray UMETA(DisplayName = "Distance Based Array"),
	DistanceBasedCurve UMETA(DisplayName = "Distance Based Curve")
};

USTRUCT(BlueprintType)
struct OPTIMIZEDSKELETALMESH_API FOptimizedSkeletalMeshDistanceUpdateRateBand
{
	GENERATED_BODY()

	FOptimizedSkeletalMeshDistanceUpdateRateBand() = default;
	FOptimizedSkeletalMeshDistanceUpdateRateBand(const float InDistance, const float InUpdateRateScale)
		: Distance(InDistance)
		, UpdateRateScale(InUpdateRateScale)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Animation", meta = (ClampMin = "0.0"))
	float Distance = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Animation", meta = (ClampMin = "0.0"))
	float UpdateRateScale = 1.0f;
};

UCLASS(Config = Game, DefaultConfig, DisplayName = "Optimized Skeletal Mesh")
class OPTIMIZEDSKELETALMESH_API UOptimizedSkeletalMeshSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
#pragma region UDeveloperSettings
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
#pragma endregion

#pragma region Animation
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Animation")
	EOptimizedSkeletalMeshDistanceBasedRateMode DistanceBasedRateMode =
		EOptimizedSkeletalMeshDistanceBasedRateMode::DistanceBasedArray;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0.0"))
	float DefaultAnimationUpdateRateHz = 30.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0.0"))
	float MinAnimationUpdateRateHz = 5.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Animation")
	bool bPauseCpuPoseWhenNotVisible = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0.0"))
	float InvisiblePoseTickRateHz = 0.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Animation", meta = (EditCondition = "DistanceBasedRateMode == EOptimizedSkeletalMeshDistanceBasedRateMode::DistanceBasedArray"))
	TArray<FOptimizedSkeletalMeshDistanceUpdateRateBand> DistanceUpdateRateBands = {
		{ 1000.0f, 1.0f },
		{ 2000.0f, 0.8f },
		{ 3000.0f, 0.5f },
		{ 5000.0f, 0.25f },
	};

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Animation", meta = (EditCondition = "DistanceBasedRateMode == EOptimizedSkeletalMeshDistanceBasedRateMode::DistanceBasedCurve"))
	TSoftObjectPtr<UCurveFloat> DistanceUpdateRateCurve;
#pragma endregion

#pragma region Rendering
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering")
	bool bDrawDebugBounds = false;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering")
	bool bDrawMeshSections = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering")
	EOptimizedSkeletalMeshDrawMode MeshDrawMode = EOptimizedSkeletalMeshDrawMode::GpuSkinnedInstanced;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering")
	bool bEnableInstanceFrustumCulling = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "1.0"))
	float InstanceCullBoundsScale = 1.5f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering")
	bool bUseConservativeProxyBounds = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "1000.0"))
	float ConservativeProxyBoundsExtent = 10000000.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering")
	bool bDrawCullingDebug = false;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering")
	bool bDrawCullTestBounds = false;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering")
	bool bCastShadows = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "0.0"))
	float NearFullShadowDistance = 1800.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "0.0"))
	float MidShadowDistance = 3200.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "1"))
	int32 MidShadowUpdateDivisor = 2;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "0"))
	int32 FarShadowUpdateDivisor = 0;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "0.0"))
	float MaxShadowCastDistance = 5000.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "0"))
	int32 MaxDynamicShadowCasters = 120;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "0"))
	int32 NearShadowLodBias = 0;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "0"))
	int32 MidShadowLodBias = 1;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "0"))
	int32 FarShadowLodBias = 2;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "0"))
	int32 MaxShadowSectionsPerLOD = 2;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "0.0"))
	float LocalLightMaxShadowCastDistance = 2000.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "0"))
	int32 LocalLightMaxDynamicShadowCasters = 24;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "0"))
	int32 LocalLightShadowLodBias = 3;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "0"))
	int32 LocalLightMaxShadowSectionsPerLOD = 1;
#pragma endregion
};
