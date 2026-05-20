// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimizedSkeletalMeshWorldSubsystem.h"

#include "Stats/Stats.h"

void UOptimizedSkeletalMeshWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Instances.Reset();
	FreeInstanceIds.Reset();
	NextInstanceId = 1;
	bRenderDataDirty = false;
}

void UOptimizedSkeletalMeshWorldSubsystem::Deinitialize()
{
	Instances.Reset();
	FreeInstanceIds.Reset();
	NextInstanceId = 1;
	bRenderDataDirty = false;

	Super::Deinitialize();
}

void UOptimizedSkeletalMeshWorldSubsystem::Tick(float DeltaTime)
{
	// Future render bridge: collect visible instances, update GPU buffers, and notify the scene proxy.
	if (bRenderDataDirty)
	{
		ClearRenderDataDirty();
	}
}

TStatId UOptimizedSkeletalMeshWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOptimizedSkeletalMeshWorldSubsystem, STATGROUP_Tickables);
}

bool UOptimizedSkeletalMeshWorldSubsystem::IsTickable() const
{
	return !IsTemplate();
}

FOptimizedSkeletalMeshInstanceHandle UOptimizedSkeletalMeshWorldSubsystem::RegisterInstance(
	const FOptimizedSkeletalMeshInstanceDesc& Desc)
{
	if (!Desc.SkeletalMesh)
	{
		return FOptimizedSkeletalMeshInstanceHandle();
	}

	const int32 InstanceId = AllocateInstanceId();
	Instances.Add(InstanceId, Desc);
	MarkRenderDataDirty();

	return FOptimizedSkeletalMeshInstanceHandle(InstanceId);
}

bool UOptimizedSkeletalMeshWorldSubsystem::UnregisterInstance(FOptimizedSkeletalMeshInstanceHandle Handle)
{
	if (!IsValidInstanceId(Handle.Id))
	{
		return false;
	}

	Instances.Remove(Handle.Id);
	FreeInstanceIds.Add(Handle.Id);
	MarkRenderDataDirty();

	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::UpdateInstance(
	FOptimizedSkeletalMeshInstanceHandle Handle,
	const FOptimizedSkeletalMeshInstanceDesc& Desc)
{
	if (!Desc.SkeletalMesh)
	{
		return false;
	}

	FOptimizedSkeletalMeshInstanceDesc* Instance = Instances.Find(Handle.Id);
	if (!Instance)
	{
		return false;
	}

	*Instance = Desc;
	MarkRenderDataDirty();

	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::UpdateInstanceTransform(
	FOptimizedSkeletalMeshInstanceHandle Handle,
	const FTransform& WorldTransform)
{
	FOptimizedSkeletalMeshInstanceDesc* Instance = Instances.Find(Handle.Id);
	if (!Instance)
	{
		return false;
	}

	Instance->WorldTransform = WorldTransform;
	MarkRenderDataDirty();

	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceVisible(
	FOptimizedSkeletalMeshInstanceHandle Handle,
	const bool bVisible)
{
	FOptimizedSkeletalMeshInstanceDesc* Instance = Instances.Find(Handle.Id);
	if (!Instance)
	{
		return false;
	}

	if (Instance->bVisible != bVisible)
	{
		Instance->bVisible = bVisible;
		MarkRenderDataDirty();
	}

	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::GetInstance(
	FOptimizedSkeletalMeshInstanceHandle Handle,
	FOptimizedSkeletalMeshInstanceDesc& OutDesc) const
{
	const FOptimizedSkeletalMeshInstanceDesc* Instance = Instances.Find(Handle.Id);
	if (!Instance)
	{
		return false;
	}

	OutDesc = *Instance;
	return true;
}

void UOptimizedSkeletalMeshWorldSubsystem::GetInstancesSnapshot(
	TArray<FOptimizedSkeletalMeshInstanceSnapshot>& OutInstances) const
{
	OutInstances.Reset(Instances.Num());

	for (const TPair<int32, FOptimizedSkeletalMeshInstanceDesc>& Pair : Instances)
	{
		FOptimizedSkeletalMeshInstanceSnapshot& Snapshot = OutInstances.AddDefaulted_GetRef();
		Snapshot.Handle = FOptimizedSkeletalMeshInstanceHandle(Pair.Key);
		Snapshot.Desc = Pair.Value;
	}
}

int32 UOptimizedSkeletalMeshWorldSubsystem::GetInstanceCount() const
{
	return Instances.Num();
}

int32 UOptimizedSkeletalMeshWorldSubsystem::AllocateInstanceId()
{
	if (!FreeInstanceIds.IsEmpty())
	{
		return FreeInstanceIds.Pop(EAllowShrinking::No);
	}

	return NextInstanceId++;
}

bool UOptimizedSkeletalMeshWorldSubsystem::IsValidInstanceId(const int32 InstanceId) const
{
	return Instances.Contains(InstanceId);
}

void UOptimizedSkeletalMeshWorldSubsystem::MarkRenderDataDirty()
{
	bRenderDataDirty = true;
}
