// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/Structs/OptimizedSkeletalMeshInstanceHandle.h"

struct FOptimizedSkeletalMeshBonePaletteSnapshot
{
	FOptimizedSkeletalMeshInstanceHandle Handle;
	TArray<FMatrix44f> PreviousBonePalette;
	TArray<FMatrix44f> BonePalette;
	float BlendAlpha = 1.0f;
};

