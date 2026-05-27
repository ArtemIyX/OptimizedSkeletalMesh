// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OptimizedSkeletalMeshInstanceDesc.generated.h"

class USkeletalMesh;
class UAnimSequence;
class UMaterialInterface;

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

