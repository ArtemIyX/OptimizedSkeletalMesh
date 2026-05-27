// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OptimizedSkeletalMeshInstanceHandle.generated.h"

USTRUCT(BlueprintType)
struct OPTIMIZEDSKELETALMESH_API FOptimizedSkeletalMeshInstanceHandle
{
	GENERATED_BODY()

public:
	FOptimizedSkeletalMeshInstanceHandle() = default;

	explicit FOptimizedSkeletalMeshInstanceHandle(const int32 InId)
		: Id(InId) {}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Optimized Skeletal Mesh")
	int32 Id = INDEX_NONE;

	bool IsValid() const
	{
		return Id != INDEX_NONE;
	}
};

