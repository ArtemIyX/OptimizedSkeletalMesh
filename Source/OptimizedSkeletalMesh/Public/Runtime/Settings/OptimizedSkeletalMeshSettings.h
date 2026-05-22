// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Engine/DeveloperSettings.h"
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
#pragma endregion

#pragma region Animation
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Animation")
	EOptimizedSkeletalMeshDistanceBasedRateMode DistanceBasedRateMode =
		EOptimizedSkeletalMeshDistanceBasedRateMode::DistanceBasedArray;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0.0"))
	float DefaultAnimationUpdateRateHz = 30.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0.0"))
	float MinAnimationUpdateRateHz = 5.0f;

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
	bool bCastShadows = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "0.0"))
	float MaxShadowCastDistance = 5000.0f;
#pragma endregion
};
