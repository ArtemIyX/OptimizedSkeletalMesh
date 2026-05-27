// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Data/Structs/OptimizedSkeletalMeshAnimationMeshCache.h"
#include "Runtime/Data/Structs/OptimizedSkeletalMeshInstanceDesc.h"

struct FOptimizedSkeletalMeshAnimationEvaluationWork
{
	int32 InstanceId = INDEX_NONE;
	FOptimizedSkeletalMeshInstanceDesc Desc;
	const FOptimizedSkeletalMeshAnimationMeshCache* MeshCache = nullptr;
};

