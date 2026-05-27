// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OptimizedSkeletonLib.generated.h"

struct FOptimizedSkeletalMeshInstanceHandle;
/**
 * 
 */
UCLASS()
class OPTIMIZEDSKELETALMESH_API UOptimizedSkeletonLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="OptimizedSkeletonLib|Make",
		DisplayName="Make Optimized Skeletal Mesh Instance Handle",
		meta=(Keywords="Make OSM Handle Optimized Skeletal Mesh Instance"))
	static FOptimizedSkeletalMeshInstanceHandle MakeOSMHandle(UPARAM(DisplayName="Instance ID") int32 InIndex);

	UFUNCTION(BlueprintCallable, Category="OptimizedSkeletonLib|Make",
		DisplayName="To Int32",
		meta=(Keywords="To int 32 OSM Handle Optimized Skeletal Mesh Instance"))
	static int32 OptimizedSkeletalMeshInstanceHandle_ToInt(const FOptimizedSkeletalMeshInstanceHandle& InHandle);
	
	UFUNCTION(BlueprintCallable, Category="OptimizedSkeletonLib|Make",
	DisplayName="Is Valid Instance",
	meta=(Keywords="Is Valid OSM Handle Optimized Skeletal Mesh Instance"))
	static bool OptimizedSkeletalMeshInstanceHandle_IsValid(const FOptimizedSkeletalMeshInstanceHandle& InHandle);
};