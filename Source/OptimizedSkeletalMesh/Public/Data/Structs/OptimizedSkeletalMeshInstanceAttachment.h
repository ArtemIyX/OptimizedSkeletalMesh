// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Data/Enums/OptimizedSkeletalMeshAttachmentFollowMode.h"
#include "Data/Structs/OptimizedSkeletalMeshInstanceHandle.h"
#include "OptimizedSkeletalMeshInstanceAttachment.generated.h"

USTRUCT(BlueprintType)
struct OPTIMIZEDSKELETALMESH_API FOptimizedSkeletalMeshInstanceAttachment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Attachment")
	FOptimizedSkeletalMeshInstanceHandle ParentHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Attachment")
	FName ParentSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Attachment", meta = (Bitmask, BitmaskEnum = "/Script/OptimizedSkeletalMesh.EOptimizedSkeletalMeshAttachmentFollowMode"))
	int32 FollowModeMask = static_cast<int32>(
		EOptimizedSkeletalMeshAttachmentFollowMode::Location | EOptimizedSkeletalMeshAttachmentFollowMode::Rotation);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Attachment")
	FTransform RelativeOffset = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Attachment")
	bool bEnabled = true;
};