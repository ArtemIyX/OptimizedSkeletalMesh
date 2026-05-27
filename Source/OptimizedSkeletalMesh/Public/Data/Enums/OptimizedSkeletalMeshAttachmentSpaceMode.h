// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OptimizedSkeletalMeshAttachmentSpaceMode.generated.h"

UENUM(BlueprintType)
enum class EOptimizedSkeletalMeshAttachmentSpaceMode : uint8
{
	Socket UMETA(DisplayName = "Socket"),
	Bone UMETA(DisplayName = "Bone"),
	MeshOrigin UMETA(DisplayName = "Mesh Origin")
};

