// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OptimizedSkeletalMeshAttachmentFollowMode.generated.h"

UENUM(BlueprintType, meta = (Bitflags))
enum class EOptimizedSkeletalMeshAttachmentFollowMode : uint8
{
	None = 0 UMETA(DisplayName = "None"),
	Location = 1 << 0 UMETA(DisplayName = "Location"),
	Rotation = 1 << 1 UMETA(DisplayName = "Rotation"),
	Scale = 1 << 2 UMETA(DisplayName = "Scale")
};
ENUM_CLASS_FLAGS(EOptimizedSkeletalMeshAttachmentFollowMode);

