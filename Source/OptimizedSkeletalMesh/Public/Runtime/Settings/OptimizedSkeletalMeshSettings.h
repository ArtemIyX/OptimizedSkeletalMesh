// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OptimizedSkeletalMeshSettings.generated.h"

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Animation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
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
	bool bEnableDistanceBasedAnimationUpdateRate = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0.0"))
	float DefaultAnimationUpdateRateHz = 30.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Animation", meta = (ClampMin = "0.0"))
	float MinAnimationUpdateRateHz = 5.0f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Animation")
	TArray<FOptimizedSkeletalMeshDistanceUpdateRateBand> DistanceUpdateRateBands = {
		{ 1000.0f, 1.0f },
		{ 2000.0f, 0.8f },
		{ 3000.0f, 0.5f },
		{ 5000.0f, 0.25f },
	};
#pragma endregion
};
