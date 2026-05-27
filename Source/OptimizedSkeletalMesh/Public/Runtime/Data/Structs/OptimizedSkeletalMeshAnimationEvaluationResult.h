// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FOptimizedSkeletalMeshAnimationEvaluationResult
{
	int32 InstanceId = INDEX_NONE;
	TArray<FMatrix44f> BonePalette;
	bool bSucceeded = false;
};

