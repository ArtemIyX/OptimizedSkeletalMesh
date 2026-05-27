// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OptimizedSkeletalMeshAttachmentFollowMode.generated.h"

/**
 * @note Uses 1 << 2 byte operations, ENUM_CLASS_FLAGS, can not be used for iteration
 */
UENUM(BlueprintType, meta = (Bitflags))
enum class EOptimizedSkeletalMeshAttachmentFollowMode : uint8
{
	Invalid = 0 UMETA(Hidden),
	Location = 1 << 0 UMETA(DisplayName = "Location"),
	Rotation = 1 << 1 UMETA(DisplayName = "Rotation"),
	Scale = 1 << 2 UMETA(DisplayName = "Scale")
};

ENUM_CLASS_FLAGS(EOptimizedSkeletalMeshAttachmentFollowMode);