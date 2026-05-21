// Copyright Epic Games, Inc. All Rights Reserved.

#include "Runtime/Manager/OptimizedSkeletalMeshWorldSubsystem.h"

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
DECLARE_DWORD_COUNTER_STAT(TEXT("Advanced Instances"), STAT_OptimizedSkeletalMeshAnimationAdvancedInstances, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Finished Instances"), STAT_OptimizedSkeletalMeshAnimationFinishedInstances, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Pose Evaluated Instances"), STAT_OptimizedSkeletalMeshAnimationPoseEvaluatedInstances, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Failed Pose Evaluations"), STAT_OptimizedSkeletalMeshAnimationFailedPoseEvaluations, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Bone Palette Instances"), STAT_OptimizedSkeletalMeshAnimationBonePaletteInstances, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Total Bone Matrices"), STAT_OptimizedSkeletalMeshAnimationTotalBoneMatrices, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Max Bones Per Instance"), STAT_OptimizedSkeletalMeshAnimationMaxBonesPerInstance, STATGROUP_OptimizedSkeletalMeshAnimation);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Last Delta Seconds"), STAT_OptimizedSkeletalMeshAnimationLastDeltaSeconds, STATGROUP_OptimizedSkeletalMeshAnimation);

namespace OptimizedSkeletalMesh
{
	static void PublishAnimationStats(const FOptimizedSkeletalMeshAnimationStats& InStats)
	{
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationRegisteredInstances, InStats.RegisteredInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationAnimatedInstances, InStats.AnimatedInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationAdvancedInstances, InStats.AdvancedInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationFinishedInstances, InStats.FinishedInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationPoseEvaluatedInstances, InStats.PoseEvaluatedInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationFailedPoseEvaluations, InStats.FailedPoseEvaluations);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationBonePaletteInstances, InStats.BonePaletteInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationTotalBoneMatrices, InStats.TotalBoneMatrices);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationMaxBonesPerInstance, InStats.MaxBonesPerInstance);
		SET_FLOAT_STAT(STAT_OptimizedSkeletalMeshAnimationLastDeltaSeconds, InStats.LastDeltaTime);
	}
} // namespace OptimizedSkeletalMesh

void UOptimizedSkeletalMeshWorldSubsystem::Initialize(FSubsystemCollectionBase& InCollection)
{
	Super::Initialize(InCollection);

	Instances.Reset();
	FreeInstanceIds.Reset();
	AnimationMeshCaches.Reset();
	InstanceBonePalettes.Reset();
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
	if (RenderComponent)
	{
		RenderComponent->PushBonePalettesToRenderThread();
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
	return !IsTemplate();
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

	instance->bPlayAnimation = bInPlaying;
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
	OutSnapshots.Reset(InstanceBonePalettes.Num());

	for (const TPair<int32, TArray<FMatrix44f>>& pair : InstanceBonePalettes)
	{
		FOptimizedSkeletalMeshBonePaletteSnapshot& snapshot = OutSnapshots.AddDefaulted_GetRef();
		snapshot.Handle = FOptimizedSkeletalMeshInstanceHandle(pair.Key);
		snapshot.BonePalette = pair.Value;
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

void UOptimizedSkeletalMeshWorldSubsystem::TickAnimation(const float InDeltaTime)
{
	FOptimizedSkeletalMeshAnimationStats newStats;
	newStats.RegisteredInstances = Instances.Num();
	newStats.LastDeltaTime = InDeltaTime;

	for (TPair<int32, FOptimizedSkeletalMeshInstanceDesc>& pair : Instances)
	{
		FOptimizedSkeletalMeshInstanceDesc& desc = pair.Value;
		if (!desc.Animation)
		{
			InstanceBonePalettes.Remove(pair.Key);
			continue;
		}

		++newStats.AnimatedInstances;

		if (InDeltaTime > 0.0f && desc.bPlayAnimation && !FMath::IsNearlyZero(desc.AnimationPlayRate))
		{
			const float sequenceLength = desc.Animation->GetPlayLength();
			if (sequenceLength > 0.0f)
			{
				const float previousTime = desc.AnimationTime;
				const float advancedTime = previousTime + InDeltaTime * desc.AnimationPlayRate;

				if (desc.bLoopAnimation)
				{
					desc.AnimationTime = WrapAnimationTime(advancedTime, sequenceLength);
				}
				else
				{
					desc.AnimationTime = FMath::Clamp(advancedTime, 0.0f, sequenceLength);
					if (desc.AnimationTime <= 0.0f || desc.AnimationTime >= sequenceLength)
					{
						desc.bPlayAnimation = false;
						++newStats.FinishedInstances;
					}
				}

				if (!FMath::IsNearlyEqual(previousTime, desc.AnimationTime))
				{
					++newStats.AdvancedInstances;
				}
			}
		}

		TArray<FMatrix44f>& bonePalette = InstanceBonePalettes.FindOrAdd(pair.Key);
		if (EvaluateInstanceBonePalette(desc, bonePalette))
		{
			++newStats.PoseEvaluatedInstances;
			++newStats.BonePaletteInstances;
			newStats.TotalBoneMatrices += bonePalette.Num();
			newStats.MaxBonesPerInstance = FMath::Max(newStats.MaxBonesPerInstance, bonePalette.Num());
		}
		else
		{
			InstanceBonePalettes.Remove(pair.Key);
			++newStats.FailedPoseEvaluations;
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

	FCompactPose localPose;
	localPose.ResetToRefPose(meshCache->RequiredBones);

	FBlendedCurve curve;
	curve.InitFrom(meshCache->RequiredBones);

	UE::Anim::FStackAttributeContainer attributes;
	FAnimationPoseData animationPoseData(localPose, curve, attributes);

	const FAnimExtractContext extractionContext(static_cast<double>(InDesc.AnimationTime), false, FDeltaTimeRecord(0.0f), InDesc.bLoopAnimation);
	InDesc.Animation->GetBonePose(animationPoseData, extractionContext);

	const int32 boneCount = meshCache->RequiredBones.GetNumBones();
	TArray<FTransform> componentSpaceTransforms;
	componentSpaceTransforms.SetNum(boneCount);

	for (const FCompactPoseBoneIndex compactBoneIndex : localPose.ForEachBoneIndex())
	{
		const FMeshPoseBoneIndex meshBoneIndex = meshCache->RequiredBones.MakeMeshPoseIndex(compactBoneIndex);
		const int32 boneIndex = meshBoneIndex.GetInt();
		if (!componentSpaceTransforms.IsValidIndex(boneIndex))
		{
			continue;
		}

		const FCompactPoseBoneIndex parentCompactBoneIndex = localPose.GetParentBoneIndex(compactBoneIndex);
		if (parentCompactBoneIndex != INDEX_NONE)
		{
			const FMeshPoseBoneIndex parentMeshBoneIndex = meshCache->RequiredBones.MakeMeshPoseIndex(parentCompactBoneIndex);
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
		OutBonePalette[boneIndex] = meshCache->RefBasesInvMatrices[boneIndex] * componentSpaceMatrix;
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
