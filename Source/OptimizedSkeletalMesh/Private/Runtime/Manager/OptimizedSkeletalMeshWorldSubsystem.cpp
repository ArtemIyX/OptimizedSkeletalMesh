// Copyright Epic Games, Inc. All Rights Reserved.

#include "Runtime/Manager/OptimizedSkeletalMeshWorldSubsystem.h"

#include "Async/ParallelFor.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"
#include "BonePose.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Runtime/Rendering/OptimizedSkeletalMeshRenderComponent.h"
#include "Stats/Stats.h"

DECLARE_STATS_GROUP_SORTBYNAME(TEXT("OptimizedSkeletalMesh Animation"), STATGROUP_OptimizedSkeletalMeshAnimation, STATCAT_Advanced);
DECLARE_DWORD_COUNTER_STAT(TEXT("Registered Instances"), STAT_OptimizedSkeletalMeshAnimationRegisteredInstances, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Animated Instances"), STAT_OptimizedSkeletalMeshAnimationAnimatedInstances, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Animation Instances"), STAT_OptimizedSkeletalMeshAnimationActiveAnimationInstances, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Dirty Animation Instances"), STAT_OptimizedSkeletalMeshAnimationDirtyAnimationInstances, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Skipped Update Rate Instances"), STAT_OptimizedSkeletalMeshAnimationSkippedUpdateRateInstances, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Parallel Pose Batches"), STAT_OptimizedSkeletalMeshAnimationParallelPoseBatches, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Advanced Instances"), STAT_OptimizedSkeletalMeshAnimationAdvancedInstances, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Finished Instances"), STAT_OptimizedSkeletalMeshAnimationFinishedInstances, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Pose Evaluated Instances"), STAT_OptimizedSkeletalMeshAnimationPoseEvaluatedInstances, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Failed Pose Evaluations"), STAT_OptimizedSkeletalMeshAnimationFailedPoseEvaluations, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Bone Palette Instances"), STAT_OptimizedSkeletalMeshAnimationBonePaletteInstances, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Render Visible Animated Instances"), STAT_OptimizedSkeletalMeshAnimationRenderVisibleAnimatedInstances, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Dirty CPU Palette Instances"), STAT_OptimizedSkeletalMeshAnimationDirtyCpuPaletteInstances, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Dirty GPU Palette Instances"), STAT_OptimizedSkeletalMeshAnimationDirtyGpuPaletteInstances, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("GPU Palette Upload Skipped Instances"), STAT_OptimizedSkeletalMeshAnimationGpuPaletteUploadSkippedInstances, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Total Bone Matrices"), STAT_OptimizedSkeletalMeshAnimationTotalBoneMatrices, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Max Bones Per Instance"), STAT_OptimizedSkeletalMeshAnimationMaxBonesPerInstance, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Last Delta Seconds"), STAT_OptimizedSkeletalMeshAnimationLastDeltaSeconds, STATGROUP_OptimizedSkeletalMeshAnimation);

namespace OptimizedSkeletalMesh
{
	static void PublishAnimationStats(const FOptimizedSkeletalMeshAnimationStats& InStats)
	{
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationRegisteredInstances, InStats.RegisteredInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationAnimatedInstances, InStats.AnimatedInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationActiveAnimationInstances, InStats.ActiveAnimationInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationDirtyAnimationInstances, InStats.DirtyAnimationInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationSkippedUpdateRateInstances, InStats.SkippedUpdateRateInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationParallelPoseBatches, InStats.ParallelPoseBatches);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationAdvancedInstances, InStats.AdvancedInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationFinishedInstances, InStats.FinishedInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationPoseEvaluatedInstances, InStats.PoseEvaluatedInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationFailedPoseEvaluations, InStats.FailedPoseEvaluations);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationBonePaletteInstances, InStats.BonePaletteInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationRenderVisibleAnimatedInstances, InStats.RenderVisibleAnimatedInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationDirtyCpuPaletteInstances, InStats.DirtyCpuPaletteInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationDirtyGpuPaletteInstances, InStats.DirtyGpuPaletteInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationGpuPaletteUploadSkippedInstances, InStats.GpuPaletteUploadSkippedInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationTotalBoneMatrices, InStats.TotalBoneMatrices);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationMaxBonesPerInstance, InStats.MaxBonesPerInstance);
		SET_FLOAT_STAT(STAT_OptimizedSkeletalMeshAnimationLastDeltaSeconds, InStats.LastDeltaTime);
	}

	struct FAnimationEvaluationWork
	{
		int32 InstanceId = INDEX_NONE;
		FOptimizedSkeletalMeshInstanceDesc Desc;
		const FOptimizedSkeletalMeshAnimationMeshCache* MeshCache = nullptr;
	};

	struct FAnimationEvaluationResult
	{
		int32 InstanceId = INDEX_NONE;
		TArray<FMatrix44f> BonePalette;
		bool bSucceeded = false;
	};
} // namespace OptimizedSkeletalMesh

void UOptimizedSkeletalMeshWorldSubsystem::Initialize(FSubsystemCollectionBase& InCollection)
{
	Super::Initialize(InCollection);

	Instances.Reset();
	FreeInstanceIds.Reset();
	AnimationMeshCaches.Reset();
	InstanceBonePalettes.Reset();
	ActiveAnimationInstanceIds.Reset();
	DirtyAnimationInstanceIds.Reset();
	DirtyBonePaletteInstanceIds.Reset();
	RenderVisibleInstanceIds.Reset();
	AnimationUpdateAccumulators.Reset();
	NextInstanceId = 1;
	bRenderDataDirty = false;
	LastAnimationStats = FOptimizedSkeletalMeshAnimationStats();
	OptimizedSkeletalMesh::PublishAnimationStats(LastAnimationStats);
	EnsureRenderBridge();
}

void UOptimizedSkeletalMeshWorldSubsystem::Deinitialize()
{
	DestroyRenderBridge();

	Instances.Reset();
	FreeInstanceIds.Reset();
	AnimationMeshCaches.Reset();
	InstanceBonePalettes.Reset();
	ActiveAnimationInstanceIds.Reset();
	DirtyAnimationInstanceIds.Reset();
	DirtyBonePaletteInstanceIds.Reset();
	RenderVisibleInstanceIds.Reset();
	AnimationUpdateAccumulators.Reset();
	NextInstanceId = 1;
	bRenderDataDirty = false;
	LastAnimationStats = FOptimizedSkeletalMeshAnimationStats();
	OptimizedSkeletalMesh::PublishAnimationStats(LastAnimationStats);

	Super::Deinitialize();
}

void UOptimizedSkeletalMeshWorldSubsystem::Tick(float InDeltaTime)
{
	EnsureRenderBridge();
	TickAnimation(InDeltaTime);
	if (RenderComponent && HasDirtyRenderVisibleBonePalettes() && RenderComponent->PushBonePalettesToRenderThread())
	{
		ClearDirtyRenderVisibleBonePalettes();
	}

	if (bRenderDataDirty)
	{
		if (RenderComponent)
		{
			RenderComponent->RequestRenderRefresh();
		}

		ClearRenderDataDirty();
	}
}

TStatId UOptimizedSkeletalMeshWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOptimizedSkeletalMeshWorldSubsystem, STATGROUP_Tickables);
}

bool UOptimizedSkeletalMeshWorldSubsystem::IsTickable() const
{
	return !IsTemplate()
		&& (bRenderDataDirty
			|| HasDirtyRenderVisibleBonePalettes()
			|| !ActiveAnimationInstanceIds.IsEmpty()
			|| !DirtyAnimationInstanceIds.IsEmpty());
}

FOptimizedSkeletalMeshInstanceHandle UOptimizedSkeletalMeshWorldSubsystem::RegisterInstance(
	const FOptimizedSkeletalMeshInstanceDesc& InDesc)
{
	if (!InDesc.SkeletalMesh)
	{
		return FOptimizedSkeletalMeshInstanceHandle();
	}

	EnsureRenderBridge();

	const int32 instanceId = AllocateInstanceId();
	Instances.Add(instanceId, InDesc);
	RefreshAnimationTracking(instanceId, InDesc, true);
	MarkRenderDataDirty();

	return FOptimizedSkeletalMeshInstanceHandle(instanceId);
}

bool UOptimizedSkeletalMeshWorldSubsystem::UnregisterInstance(FOptimizedSkeletalMeshInstanceHandle InHandle)
{
	if (!IsValidInstanceId(InHandle.Id))
	{
		return false;
	}

	Instances.Remove(InHandle.Id);
	InstanceBonePalettes.Remove(InHandle.Id);
	RemoveAnimationTracking(InHandle.Id);
	MarkBonePaletteDirty(InHandle.Id);
	FreeInstanceIds.Add(InHandle.Id);
	MarkRenderDataDirty();

	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::UpdateInstance(
	FOptimizedSkeletalMeshInstanceHandle InHandle,
	const FOptimizedSkeletalMeshInstanceDesc& InDesc)
{
	if (!InDesc.SkeletalMesh)
	{
		return false;
	}

	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	*instance = InDesc;
	RefreshAnimationTracking(InHandle.Id, InDesc, true);
	MarkRenderDataDirty();

	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::UpdateInstanceTransform(
	FOptimizedSkeletalMeshInstanceHandle InHandle,
	const FTransform& InWorldTransform)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	instance->WorldTransform = InWorldTransform;
	MarkRenderDataDirty();

	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::UpdateInstanceAnimationTime(
	FOptimizedSkeletalMeshInstanceHandle InHandle,
	const float InAnimationTime)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	instance->AnimationTime = FMath::Max(0.0f, InAnimationTime);
	RefreshAnimationTracking(InHandle.Id, *instance, true);
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceAnimationPlaying(
	FOptimizedSkeletalMeshInstanceHandle InHandle,
	const bool bInPlaying)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	if (instance->bPlayAnimation != bInPlaying)
	{
		instance->bPlayAnimation = bInPlaying;
		RefreshAnimationTracking(InHandle.Id, *instance, true);
	}
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceVisible(
	FOptimizedSkeletalMeshInstanceHandle InHandle,
	const bool bInVisible)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	if (instance->bVisible != bInVisible)
	{
		instance->bVisible = bInVisible;
		MarkRenderDataDirty();
	}

	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::ShowInstance(const FOptimizedSkeletalMeshInstanceHandle InHandle)
{
	return SetInstanceVisible(InHandle, true);
}

bool UOptimizedSkeletalMeshWorldSubsystem::HideInstance(const FOptimizedSkeletalMeshInstanceHandle InHandle)
{
	return SetInstanceVisible(InHandle, false);
}

int32 UOptimizedSkeletalMeshWorldSubsystem::SetInstancesVisible(
	const TArray<FOptimizedSkeletalMeshInstanceHandle>& InHandles,
	const bool bInVisible)
{
	int32 updatedCount = 0;

	for (const FOptimizedSkeletalMeshInstanceHandle& handle : InHandles)
	{
		FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(handle.Id);
		if (!instance || instance->bVisible == bInVisible)
		{
			continue;
		}

		instance->bVisible = bInVisible;
		++updatedCount;
	}

	if (updatedCount > 0)
	{
		MarkRenderDataDirty();
	}

	return updatedCount;
}

bool UOptimizedSkeletalMeshWorldSubsystem::GetInstance(
	FOptimizedSkeletalMeshInstanceHandle InHandle,
	FOptimizedSkeletalMeshInstanceDesc& OutDesc) const
{
	const FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	OutDesc = *instance;
	return true;
}

void UOptimizedSkeletalMeshWorldSubsystem::GetInstancesSnapshot(
	TArray<FOptimizedSkeletalMeshInstanceSnapshot>& OutInstances) const
{
	OutInstances.Reset(Instances.Num());

	for (const TPair<int32, FOptimizedSkeletalMeshInstanceDesc>& pair : Instances)
	{
		FOptimizedSkeletalMeshInstanceSnapshot& snapshot = OutInstances.AddDefaulted_GetRef();
		snapshot.Handle = FOptimizedSkeletalMeshInstanceHandle(pair.Key);
		snapshot.Desc = pair.Value;
	}

	OutInstances.Sort(
		[](const FOptimizedSkeletalMeshInstanceSnapshot& InLeft, const FOptimizedSkeletalMeshInstanceSnapshot& InRight) {
			return InLeft.Handle.Id < InRight.Handle.Id;
		});
}

int32 UOptimizedSkeletalMeshWorldSubsystem::GetInstanceCount() const
{
	return Instances.Num();
}

int32 UOptimizedSkeletalMeshWorldSubsystem::GetVisibleRenderBatchCount() const
{
	TSet<USkeletalMesh*> visibleMeshes;

	for (const TPair<int32, FOptimizedSkeletalMeshInstanceDesc>& pair : Instances)
	{
		const FOptimizedSkeletalMeshInstanceDesc& desc = pair.Value;
		if (!desc.bVisible || !desc.SkeletalMesh)
		{
			continue;
		}

		visibleMeshes.Add(desc.SkeletalMesh.Get());
	}

	return visibleMeshes.Num();
}

FOptimizedSkeletalMeshAnimationStats UOptimizedSkeletalMeshWorldSubsystem::GetLastAnimationStats() const
{
	return LastAnimationStats;
}

const TArray<FMatrix44f>* UOptimizedSkeletalMeshWorldSubsystem::GetInstanceBonePalette(
	const FOptimizedSkeletalMeshInstanceHandle InHandle) const
{
	return InstanceBonePalettes.Find(InHandle.Id);
}

void UOptimizedSkeletalMeshWorldSubsystem::GetBonePaletteSnapshots(
	TArray<FOptimizedSkeletalMeshBonePaletteSnapshot>& OutSnapshots) const
{
	OutSnapshots.Reset(RenderVisibleInstanceIds.Num());

	for (const int32 instanceId : RenderVisibleInstanceIds)
	{
		const TArray<FMatrix44f>* bonePalette = InstanceBonePalettes.Find(instanceId);
		if (!bonePalette || bonePalette->IsEmpty())
		{
			continue;
		}

		FOptimizedSkeletalMeshBonePaletteSnapshot& snapshot = OutSnapshots.AddDefaulted_GetRef();
		snapshot.Handle = FOptimizedSkeletalMeshInstanceHandle(instanceId);
		snapshot.BonePalette = *bonePalette;
	}

	OutSnapshots.Sort(
		[](const FOptimizedSkeletalMeshBonePaletteSnapshot& InLeft, const FOptimizedSkeletalMeshBonePaletteSnapshot& InRight) {
			return InLeft.Handle.Id < InRight.Handle.Id;
		});
}

int32 UOptimizedSkeletalMeshWorldSubsystem::GetCachedBonePaletteCount() const
{
	return InstanceBonePalettes.Num();
}

void UOptimizedSkeletalMeshWorldSubsystem::UpdateRenderVisibleInstanceIds(
	const TConstArrayView<int32> InVisibleInstanceIds)
{
	TSet<int32> newVisibleInstanceIds;
	newVisibleInstanceIds.Reserve(InVisibleInstanceIds.Num());
	for (const int32 instanceId : InVisibleInstanceIds)
	{
		if (Instances.Contains(instanceId))
		{
			newVisibleInstanceIds.Add(instanceId);
		}
	}

	for (const int32 instanceId : newVisibleInstanceIds)
	{
		if (!RenderVisibleInstanceIds.Contains(instanceId) && InstanceBonePalettes.Contains(instanceId))
		{
			MarkBonePaletteDirty(instanceId);
		}
	}

	RenderVisibleInstanceIds = MoveTemp(newVisibleInstanceIds);
}

void UOptimizedSkeletalMeshWorldSubsystem::SetExternalRenderBridgeActive(const bool bInActive)
{
	bExternalRenderBridgeActive = bInActive;

	if (bExternalRenderBridgeActive)
	{
		DestroyRenderBridge();
	}
	else
	{
		EnsureRenderBridge();
	}
}

void UOptimizedSkeletalMeshWorldSubsystem::EnsureRenderBridge()
{
	if (bExternalRenderBridgeActive)
	{
		return;
	}

	if (RenderComponent && RenderComponent->IsRegistered())
	{
		return;
	}

	UWorld* world = GetWorld();
	if (!world || world->bIsTearingDown)
	{
		return;
	}

	if (!RenderBridgeActor)
	{
		FActorSpawnParameters spawnParameters;
		spawnParameters.Name = MakeUniqueObjectName(world->PersistentLevel, AActor::StaticClass(), TEXT("OptimizedSkeletalMeshRenderBridge"));
		spawnParameters.ObjectFlags |= RF_Transient;
		spawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		RenderBridgeActor = world->SpawnActor<AActor>(spawnParameters);
		if (RenderBridgeActor)
		{
			RenderBridgeActor->SetCanBeDamaged(false);
			RenderBridgeActor->SetActorEnableCollision(false);
		}
	}

	if (RenderBridgeActor && !RenderComponent)
	{
		RenderComponent = NewObject<UOptimizedSkeletalMeshRenderComponent>(
			RenderBridgeActor,
			TEXT("OptimizedSkeletalMeshRenderComponent"),
			RF_Transient);
		RenderComponent->SetOptimizedSkeletalMeshSubsystem(this);
		RenderBridgeActor->AddInstanceComponent(RenderComponent);
		RenderComponent->RegisterComponentWithWorld(world);
	}
}

void UOptimizedSkeletalMeshWorldSubsystem::DestroyRenderBridge()
{
	if (RenderComponent)
	{
		if (RenderComponent->IsRegistered())
		{
			RenderComponent->UnregisterComponent();
		}

		RenderComponent = nullptr;
	}

	if (RenderBridgeActor)
	{
		RenderBridgeActor->Destroy();
		RenderBridgeActor = nullptr;
	}
}

int32 UOptimizedSkeletalMeshWorldSubsystem::AllocateInstanceId()
{
	if (!FreeInstanceIds.IsEmpty())
	{
		return FreeInstanceIds.Pop(EAllowShrinking::No);
	}

	return NextInstanceId++;
}

bool UOptimizedSkeletalMeshWorldSubsystem::IsValidInstanceId(const int32 InInstanceId) const
{
	return Instances.Contains(InInstanceId);
}

void UOptimizedSkeletalMeshWorldSubsystem::MarkRenderDataDirty()
{
	bRenderDataDirty = true;

	if (RenderComponent)
	{
		RenderComponent->RequestRenderRefresh();
	}
}

void UOptimizedSkeletalMeshWorldSubsystem::RefreshAnimationTracking(
	const int32 InInstanceId,
	const FOptimizedSkeletalMeshInstanceDesc& InDesc,
	const bool bInForceDirty)
{
	if (!InDesc.Animation || !InDesc.SkeletalMesh)
	{
		RemoveAnimationTracking(InInstanceId);
		if (InstanceBonePalettes.Remove(InInstanceId) > 0)
		{
			MarkBonePaletteDirty(InInstanceId);
		}
		return;
	}

	if (ShouldTickAnimation(InDesc))
	{
		ActiveAnimationInstanceIds.Add(InInstanceId);
	}
	else
	{
		ActiveAnimationInstanceIds.Remove(InInstanceId);
	}

	if (bInForceDirty)
	{
		DirtyAnimationInstanceIds.Add(InInstanceId);
	}
}

void UOptimizedSkeletalMeshWorldSubsystem::RemoveAnimationTracking(const int32 InInstanceId)
{
	ActiveAnimationInstanceIds.Remove(InInstanceId);
	DirtyAnimationInstanceIds.Remove(InInstanceId);
	DirtyBonePaletteInstanceIds.Remove(InInstanceId);
	RenderVisibleInstanceIds.Remove(InInstanceId);
	AnimationUpdateAccumulators.Remove(InInstanceId);
}

void UOptimizedSkeletalMeshWorldSubsystem::MarkBonePaletteDirty(const int32 InInstanceId)
{
	DirtyBonePaletteInstanceIds.Add(InInstanceId);
}

bool UOptimizedSkeletalMeshWorldSubsystem::HasDirtyRenderVisibleBonePalettes() const
{
	for (const int32 instanceId : DirtyBonePaletteInstanceIds)
	{
		if (RenderVisibleInstanceIds.Contains(instanceId))
		{
			return true;
		}
	}

	return false;
}

void UOptimizedSkeletalMeshWorldSubsystem::ClearDirtyRenderVisibleBonePalettes()
{
	for (auto iterator = DirtyBonePaletteInstanceIds.CreateIterator(); iterator; ++iterator)
	{
		if (RenderVisibleInstanceIds.Contains(*iterator))
		{
			iterator.RemoveCurrent();
		}
	}
}

bool UOptimizedSkeletalMeshWorldSubsystem::ShouldTickAnimation(
	const FOptimizedSkeletalMeshInstanceDesc& InDesc)
{
	return InDesc.SkeletalMesh
		&& InDesc.Animation
		&& InDesc.bPlayAnimation
		&& !FMath::IsNearlyZero(InDesc.AnimationPlayRate);
}

void UOptimizedSkeletalMeshWorldSubsystem::TickAnimation(const float InDeltaTime)
{
	FOptimizedSkeletalMeshAnimationStats newStats;
	newStats.RegisteredInstances = Instances.Num();
	newStats.LastDeltaTime = InDeltaTime;
	newStats.ActiveAnimationInstances = ActiveAnimationInstanceIds.Num();
	newStats.DirtyAnimationInstances = DirtyAnimationInstanceIds.Num();
	newStats.RenderVisibleAnimatedInstances = RenderVisibleInstanceIds.Num();
	newStats.DirtyCpuPaletteInstances = DirtyBonePaletteInstanceIds.Num();
	newStats.DirtyGpuPaletteInstances = 0;
	for (const int32 instanceId : DirtyBonePaletteInstanceIds)
	{
		if (RenderVisibleInstanceIds.Contains(instanceId))
		{
			++newStats.DirtyGpuPaletteInstances;
		}
		else
		{
			++newStats.GpuPaletteUploadSkippedInstances;
		}
	}

	if (ActiveAnimationInstanceIds.IsEmpty() && DirtyAnimationInstanceIds.IsEmpty())
	{
		newStats.BonePaletteInstances = InstanceBonePalettes.Num();
		for (const TPair<int32, TArray<FMatrix44f>>& pair : InstanceBonePalettes)
		{
			newStats.TotalBoneMatrices += pair.Value.Num();
			newStats.MaxBonesPerInstance = FMath::Max(newStats.MaxBonesPerInstance, pair.Value.Num());
		}

		newStats.DirtyCpuPaletteInstances = DirtyBonePaletteInstanceIds.Num();
		newStats.DirtyGpuPaletteInstances = 0;
		newStats.GpuPaletteUploadSkippedInstances = 0;
		for (const int32 instanceId : DirtyBonePaletteInstanceIds)
		{
			if (RenderVisibleInstanceIds.Contains(instanceId))
			{
				++newStats.DirtyGpuPaletteInstances;
			}
			else
			{
				++newStats.GpuPaletteUploadSkippedInstances;
			}
		}

		LastAnimationStats = newStats;
		OptimizedSkeletalMesh::PublishAnimationStats(LastAnimationStats);
		return;
	}

	TArray<int32> instanceIdsToProcess;
	instanceIdsToProcess.Reserve(ActiveAnimationInstanceIds.Num() + DirtyAnimationInstanceIds.Num());
	for (const int32 instanceId : DirtyAnimationInstanceIds)
	{
		instanceIdsToProcess.AddUnique(instanceId);
	}
	for (const int32 instanceId : ActiveAnimationInstanceIds)
	{
		instanceIdsToProcess.AddUnique(instanceId);
	}

	TArray<OptimizedSkeletalMesh::FAnimationEvaluationWork> evaluationWork;
	evaluationWork.Reserve(instanceIdsToProcess.Num());

	for (const int32 instanceId : instanceIdsToProcess)
	{
		FOptimizedSkeletalMeshInstanceDesc* desc = Instances.Find(instanceId);
		if (!desc)
		{
			RemoveAnimationTracking(instanceId);
			continue;
		}

		if (!desc->Animation || !desc->SkeletalMesh)
		{
			if (InstanceBonePalettes.Remove(instanceId) > 0)
			{
				MarkBonePaletteDirty(instanceId);
			}
			RemoveAnimationTracking(instanceId);
			continue;
		}

		++newStats.AnimatedInstances;
		bool bShouldEvaluate = DirtyAnimationInstanceIds.Contains(instanceId);

		if (ActiveAnimationInstanceIds.Contains(instanceId))
		{
			float animationDeltaTime = InDeltaTime;
			if (desc->AnimationUpdateRateHz > 0.0f)
			{
				const float updateInterval = 1.0f / desc->AnimationUpdateRateHz;
				float& accumulator = AnimationUpdateAccumulators.FindOrAdd(instanceId);
				accumulator += InDeltaTime;
				if (accumulator < updateInterval && !bShouldEvaluate)
				{
					++newStats.SkippedUpdateRateInstances;
					continue;
				}

				animationDeltaTime = accumulator;
				accumulator = 0.0f;
			}

			if (animationDeltaTime > 0.0f)
			{
				const float sequenceLength = desc->Animation->GetPlayLength();
				if (sequenceLength > 0.0f)
				{
					const float previousTime = desc->AnimationTime;
					const float advancedTime = previousTime + animationDeltaTime * desc->AnimationPlayRate;

					if (desc->bLoopAnimation)
					{
						desc->AnimationTime = WrapAnimationTime(advancedTime, sequenceLength);
					}
					else
					{
						desc->AnimationTime = FMath::Clamp(advancedTime, 0.0f, sequenceLength);
						if (desc->AnimationTime <= 0.0f || desc->AnimationTime >= sequenceLength)
						{
							desc->bPlayAnimation = false;
							ActiveAnimationInstanceIds.Remove(instanceId);
							++newStats.FinishedInstances;
						}
					}

					if (!FMath::IsNearlyEqual(previousTime, desc->AnimationTime))
					{
						++newStats.AdvancedInstances;
						bShouldEvaluate = true;
					}
				}
			}
		}

		if (!bShouldEvaluate)
		{
			continue;
		}

		FOptimizedSkeletalMeshAnimationMeshCache* meshCache = FindOrBuildAnimationMeshCache(desc->SkeletalMesh);
		if (!meshCache)
		{
			if (InstanceBonePalettes.Remove(instanceId) > 0)
			{
				MarkBonePaletteDirty(instanceId);
			}
			++newStats.FailedPoseEvaluations;
			continue;
		}

		OptimizedSkeletalMesh::FAnimationEvaluationWork& work = evaluationWork.AddDefaulted_GetRef();
		work.InstanceId = instanceId;
		work.Desc = *desc;
		work.MeshCache = meshCache;
	}

	TArray<OptimizedSkeletalMesh::FAnimationEvaluationResult> evaluationResults;
	evaluationResults.SetNum(evaluationWork.Num());

	constexpr int32 ParallelEvaluationThreshold = 32;
	if (evaluationWork.Num() >= ParallelEvaluationThreshold)
	{
		++newStats.ParallelPoseBatches;
		ParallelFor(
			evaluationWork.Num(),
			[&evaluationWork, &evaluationResults](const int32 InWorkIndex) {
				const OptimizedSkeletalMesh::FAnimationEvaluationWork& work = evaluationWork[InWorkIndex];
				OptimizedSkeletalMesh::FAnimationEvaluationResult& result = evaluationResults[InWorkIndex];
				result.InstanceId = work.InstanceId;
				result.bSucceeded =
					work.MeshCache
					&& UOptimizedSkeletalMeshWorldSubsystem::EvaluateInstanceBonePaletteWithCache(
						work.Desc,
						*work.MeshCache,
						result.BonePalette);
			});
	}
	else
	{
		for (int32 workIndex = 0; workIndex < evaluationWork.Num(); ++workIndex)
		{
			const OptimizedSkeletalMesh::FAnimationEvaluationWork& work = evaluationWork[workIndex];
			OptimizedSkeletalMesh::FAnimationEvaluationResult& result = evaluationResults[workIndex];
			result.InstanceId = work.InstanceId;
			result.bSucceeded =
				work.MeshCache
				&& EvaluateInstanceBonePaletteWithCache(
					work.Desc,
					*work.MeshCache,
					result.BonePalette);
		}
	}

	for (OptimizedSkeletalMesh::FAnimationEvaluationResult& result : evaluationResults)
	{
		if (result.bSucceeded)
		{
			++newStats.PoseEvaluatedInstances;
			TArray<FMatrix44f>& bonePalette = InstanceBonePalettes.FindOrAdd(result.InstanceId);
			bonePalette = MoveTemp(result.BonePalette);
			MarkBonePaletteDirty(result.InstanceId);
		}
		else
		{
			if (InstanceBonePalettes.Remove(result.InstanceId) > 0)
			{
				MarkBonePaletteDirty(result.InstanceId);
			}
			++newStats.FailedPoseEvaluations;
		}
	}

	DirtyAnimationInstanceIds.Reset();

	newStats.BonePaletteInstances = InstanceBonePalettes.Num();
	for (const TPair<int32, TArray<FMatrix44f>>& pair : InstanceBonePalettes)
	{
		newStats.TotalBoneMatrices += pair.Value.Num();
		newStats.MaxBonesPerInstance = FMath::Max(newStats.MaxBonesPerInstance, pair.Value.Num());
	}
	newStats.ActiveAnimationInstances = ActiveAnimationInstanceIds.Num();
	newStats.DirtyAnimationInstances = DirtyAnimationInstanceIds.Num();
	newStats.RenderVisibleAnimatedInstances = RenderVisibleInstanceIds.Num();
	newStats.DirtyCpuPaletteInstances = DirtyBonePaletteInstanceIds.Num();
	newStats.DirtyGpuPaletteInstances = 0;
	newStats.GpuPaletteUploadSkippedInstances = 0;
	for (const int32 instanceId : DirtyBonePaletteInstanceIds)
	{
		if (RenderVisibleInstanceIds.Contains(instanceId))
		{
			++newStats.DirtyGpuPaletteInstances;
		}
		else
		{
			++newStats.GpuPaletteUploadSkippedInstances;
		}
	}

	LastAnimationStats = newStats;
	OptimizedSkeletalMesh::PublishAnimationStats(LastAnimationStats);
}

FOptimizedSkeletalMeshAnimationMeshCache* UOptimizedSkeletalMeshWorldSubsystem::FindOrBuildAnimationMeshCache(
	USkeletalMesh* InSkeletalMesh)
{
	if (!InSkeletalMesh)
	{
		return nullptr;
	}

	const TObjectKey<USkeletalMesh> meshKey(InSkeletalMesh);
	if (FOptimizedSkeletalMeshAnimationMeshCache* existingCache = AnimationMeshCaches.Find(meshKey))
	{
		return existingCache;
	}

	const FReferenceSkeleton& refSkeleton = InSkeletalMesh->GetRefSkeleton();
	const int32 boneCount = refSkeleton.GetNum();
	if (boneCount <= 0)
	{
		return nullptr;
	}

	FOptimizedSkeletalMeshAnimationMeshCache& newCache = AnimationMeshCaches.Add(meshKey);
	newCache.RequiredBoneIndices.Reset(boneCount);
	newCache.RequiredBoneIndices.Reserve(boneCount);
	for (int32 boneIndex = 0; boneIndex < boneCount; ++boneIndex)
	{
		newCache.RequiredBoneIndices.Add(static_cast<FBoneIndexType>(boneIndex));
	}

	newCache.RequiredBones.InitializeTo(
		newCache.RequiredBoneIndices,
		UE::Anim::FCurveFilterSettings(UE::Anim::ECurveFilterMode::DisallowAll),
		*InSkeletalMesh);

	TArray<FTransform> refComponentSpaceTransforms;
	refComponentSpaceTransforms.SetNum(boneCount);
	const TArray<FTransform>& refLocalTransforms = refSkeleton.GetRefBonePose();
	for (int32 boneIndex = 0; boneIndex < boneCount; ++boneIndex)
	{
		const int32 parentIndex = refSkeleton.GetParentIndex(boneIndex);
		if (parentIndex != INDEX_NONE)
		{
			refComponentSpaceTransforms[boneIndex] = refLocalTransforms[boneIndex] * refComponentSpaceTransforms[parentIndex];
		}
		else
		{
			refComponentSpaceTransforms[boneIndex] = refLocalTransforms[boneIndex];
		}
	}

	newCache.RefBasesInvMatrices.SetNum(boneCount);
	for (int32 boneIndex = 0; boneIndex < boneCount; ++boneIndex)
	{
		newCache.RefBasesInvMatrices[boneIndex] = FMatrix44f(refComponentSpaceTransforms[boneIndex].ToMatrixWithScale().Inverse());
	}

	return &newCache;
}

bool UOptimizedSkeletalMeshWorldSubsystem::EvaluateInstanceBonePalette(
	const FOptimizedSkeletalMeshInstanceDesc& InDesc,
	TArray<FMatrix44f>& OutBonePalette)
{
	if (!InDesc.SkeletalMesh || !InDesc.Animation)
	{
		OutBonePalette.Reset();
		return false;
	}

	FOptimizedSkeletalMeshAnimationMeshCache* meshCache = FindOrBuildAnimationMeshCache(InDesc.SkeletalMesh);
	if (!meshCache || meshCache->RequiredBoneIndices.IsEmpty())
	{
		OutBonePalette.Reset();
		return false;
	}

	return EvaluateInstanceBonePaletteWithCache(InDesc, *meshCache, OutBonePalette);
}

bool UOptimizedSkeletalMeshWorldSubsystem::EvaluateInstanceBonePaletteWithCache(
	const FOptimizedSkeletalMeshInstanceDesc& InDesc,
	const FOptimizedSkeletalMeshAnimationMeshCache& InMeshCache,
	TArray<FMatrix44f>& OutBonePalette)
{
	if (!InDesc.SkeletalMesh || !InDesc.Animation || InMeshCache.RequiredBoneIndices.IsEmpty())
	{
		OutBonePalette.Reset();
		return false;
	}

	FCompactPose localPose;
	localPose.ResetToRefPose(InMeshCache.RequiredBones);

	FBlendedCurve curve;
	curve.InitFrom(InMeshCache.RequiredBones);

	UE::Anim::FStackAttributeContainer attributes;
	FAnimationPoseData animationPoseData(localPose, curve, attributes);

	const FAnimExtractContext extractionContext(static_cast<double>(InDesc.AnimationTime), false, FDeltaTimeRecord(0.0f), InDesc.bLoopAnimation);
	InDesc.Animation->GetBonePose(animationPoseData, extractionContext);

	const int32 boneCount = InMeshCache.RequiredBones.GetNumBones();
	TArray<FTransform> componentSpaceTransforms;
	componentSpaceTransforms.SetNum(boneCount);

	for (const FCompactPoseBoneIndex compactBoneIndex : localPose.ForEachBoneIndex())
	{
		const FMeshPoseBoneIndex meshBoneIndex = InMeshCache.RequiredBones.MakeMeshPoseIndex(compactBoneIndex);
		const int32 boneIndex = meshBoneIndex.GetInt();
		if (!componentSpaceTransforms.IsValidIndex(boneIndex))
		{
			continue;
		}

		const FCompactPoseBoneIndex parentCompactBoneIndex = localPose.GetParentBoneIndex(compactBoneIndex);
		if (parentCompactBoneIndex != INDEX_NONE)
		{
			const FMeshPoseBoneIndex parentMeshBoneIndex = InMeshCache.RequiredBones.MakeMeshPoseIndex(parentCompactBoneIndex);
			componentSpaceTransforms[boneIndex] = localPose[compactBoneIndex] * componentSpaceTransforms[parentMeshBoneIndex.GetInt()];
		}
		else
		{
			componentSpaceTransforms[boneIndex] = localPose[compactBoneIndex];
		}
	}

	OutBonePalette.SetNum(boneCount);
	for (int32 boneIndex = 0; boneIndex < boneCount; ++boneIndex)
	{
		const FMatrix44f componentSpaceMatrix(componentSpaceTransforms[boneIndex].ToMatrixWithScale());
		OutBonePalette[boneIndex] = InMeshCache.RefBasesInvMatrices[boneIndex] * componentSpaceMatrix;
	}

	return true;
}

float UOptimizedSkeletalMeshWorldSubsystem::WrapAnimationTime(
	const float InAnimationTime,
	const float InSequenceLength)
{
	if (InSequenceLength <= 0.0f)
	{
		return 0.0f;
	}

	float wrappedTime = FMath::Fmod(InAnimationTime, InSequenceLength);
	if (wrappedTime < 0.0f)
	{
		wrappedTime += InSequenceLength;
	}

	return wrappedTime;
}
