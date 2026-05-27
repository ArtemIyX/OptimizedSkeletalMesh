// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/Structs/OptimizedSkeletalMeshInstanceDesc.h"
#include "Data/Structs/OptimizedSkeletalMeshInstanceHandle.h"
#include "OptimizedSkeletalMeshInstanceSnapshot.generated.h"

USTRUCT(BlueprintType)
struct OPTIMIZEDSKELETALMESH_API FOptimizedSkeletalMeshInstanceSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh")
	FOptimizedSkeletalMeshInstanceHandle Handle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh")
	FOptimizedSkeletalMeshInstanceDesc Desc;
};

