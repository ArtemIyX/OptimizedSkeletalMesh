// Fill out your copyright notice in the Description page of Project Settings.


#include "Libs/OptimizedSkeletonLib.h"

#include "Data/Structs/OptimizedSkeletalMeshInstanceHandle.h"

FOptimizedSkeletalMeshInstanceHandle UOptimizedSkeletonLib::MakeOSMHandle(int32 InIndex)
{
	return FOptimizedSkeletalMeshInstanceHandle(InIndex);
}

int32 UOptimizedSkeletonLib::OptimizedSkeletalMeshInstanceHandle_ToInt(const FOptimizedSkeletalMeshInstanceHandle& InHandle)
{
	return InHandle.Id;
}

bool UOptimizedSkeletonLib::OptimizedSkeletalMeshInstanceHandle_IsValid(const FOptimizedSkeletalMeshInstanceHandle& InHandle)
{
	return InHandle.IsValid();
}