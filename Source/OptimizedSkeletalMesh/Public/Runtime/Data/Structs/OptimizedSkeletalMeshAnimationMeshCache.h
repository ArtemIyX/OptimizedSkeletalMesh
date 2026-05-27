// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoneContainer.h"
#include "CoreMinimal.h"

struct FOptimizedSkeletalMeshAnimationMeshCache
{
	TArray<FBoneIndexType> RequiredBoneIndices;
	FBoneContainer RequiredBones;
	TArray<FMatrix44f> RefBasesInvMatrices;
};
