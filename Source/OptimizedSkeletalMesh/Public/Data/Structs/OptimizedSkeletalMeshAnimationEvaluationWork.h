// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/Structs/OptimizedSkeletalMeshAnimationMeshCache.h"
#include "Data/Structs/OptimizedSkeletalMeshInstanceDesc.h"

struct FOptimizedSkeletalMeshAnimationEvaluationWork
{
	int32 InstanceId = INDEX_NONE;
	FOptimizedSkeletalMeshInstanceDesc Desc;
	const FOptimizedSkeletalMeshAnimationMeshCache* MeshCache = nullptr;
};

