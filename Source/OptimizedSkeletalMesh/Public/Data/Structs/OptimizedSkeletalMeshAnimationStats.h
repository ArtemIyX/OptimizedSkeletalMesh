// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OptimizedSkeletalMeshAnimationStats.generated.h"

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

