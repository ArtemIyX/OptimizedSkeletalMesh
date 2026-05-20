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
}

void UOptimizedSkeletalMeshWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

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

void UOptimizedSkeletalMeshWorldSubsystem::Tick(float DeltaTime)
{
	EnsureRenderBridge();
	TickAnimation(DeltaTime);

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
	const FOptimizedSkeletalMeshInstanceDesc& Desc)
{
	if (!Desc.SkeletalMesh)
	{
		return FOptimizedSkeletalMeshInstanceHandle();
	}

	EnsureRenderBridge();

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
	InstanceBonePalettes.Remove(Handle.Id);
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

bool UOptimizedSkeletalMeshWorldSubsystem::UpdateInstanceAnimationTime(
	FOptimizedSkeletalMeshInstanceHandle Handle,
	const float AnimationTime)
{
	FOptimizedSkeletalMeshInstanceDesc* Instance = Instances.Find(Handle.Id);
	if (!Instance)
	{
		return false;
	}

	Instance->AnimationTime = FMath::Max(0.0f, AnimationTime);
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceAnimationPlaying(
	FOptimizedSkeletalMeshInstanceHandle Handle,
	const bool bPlaying)
{
	FOptimizedSkeletalMeshInstanceDesc* Instance = Instances.Find(Handle.Id);
	if (!Instance)
	{
		return false;
	}

	Instance->bPlayAnimation = bPlaying;
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

bool UOptimizedSkeletalMeshWorldSubsystem::ShowInstance(const FOptimizedSkeletalMeshInstanceHandle Handle)
{
	return SetInstanceVisible(Handle, true);
}

bool UOptimizedSkeletalMeshWorldSubsystem::HideInstance(const FOptimizedSkeletalMeshInstanceHandle Handle)
{
	return SetInstanceVisible(Handle, false);
}

int32 UOptimizedSkeletalMeshWorldSubsystem::SetInstancesVisible(
	const TArray<FOptimizedSkeletalMeshInstanceHandle>& Handles,
	const bool bVisible)
{
	int32 UpdatedCount = 0;

	for (const FOptimizedSkeletalMeshInstanceHandle& Handle : Handles)
	{
		FOptimizedSkeletalMeshInstanceDesc* Instance = Instances.Find(Handle.Id);
		if (!Instance || Instance->bVisible == bVisible)
		{
			continue;
		}

		Instance->bVisible = bVisible;
		++UpdatedCount;
	}

	if (UpdatedCount > 0)
	{
		MarkRenderDataDirty();
	}

	return UpdatedCount;
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

	OutInstances.Sort(
		[](const FOptimizedSkeletalMeshInstanceSnapshot& Left, const FOptimizedSkeletalMeshInstanceSnapshot& Right)
		{
			return Left.Handle.Id < Right.Handle.Id;
		});
}

int32 UOptimizedSkeletalMeshWorldSubsystem::GetInstanceCount() const
{
	return Instances.Num();
}

int32 UOptimizedSkeletalMeshWorldSubsystem::GetVisibleRenderBatchCount() const
{
	TSet<USkeletalMesh*> VisibleMeshes;

	for (const TPair<int32, FOptimizedSkeletalMeshInstanceDesc>& Pair : Instances)
	{
		const FOptimizedSkeletalMeshInstanceDesc& Desc = Pair.Value;
		if (!Desc.bVisible || !Desc.SkeletalMesh)
		{
			continue;
		}

		VisibleMeshes.Add(Desc.SkeletalMesh.Get());
	}

	return VisibleMeshes.Num();
}

FOptimizedSkeletalMeshAnimationStats UOptimizedSkeletalMeshWorldSubsystem::GetLastAnimationStats() const
{
	return LastAnimationStats;
}

const TArray<FMatrix44f>* UOptimizedSkeletalMeshWorldSubsystem::GetInstanceBonePalette(
	const FOptimizedSkeletalMeshInstanceHandle Handle) const
{
	return InstanceBonePalettes.Find(Handle.Id);
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

	UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		return;
	}

	if (!RenderBridgeActor)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = MakeUniqueObjectName(World->PersistentLevel, AActor::StaticClass(), TEXT("OptimizedSkeletalMeshRenderBridge"));
		SpawnParameters.ObjectFlags |= RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		RenderBridgeActor = World->SpawnActor<AActor>(SpawnParameters);
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
		RenderComponent->RegisterComponentWithWorld(World);
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

bool UOptimizedSkeletalMeshWorldSubsystem::IsValidInstanceId(const int32 InstanceId) const
{
	return Instances.Contains(InstanceId);
}

void UOptimizedSkeletalMeshWorldSubsystem::MarkRenderDataDirty()
{
	bRenderDataDirty = true;

	if (RenderComponent)
	{
		RenderComponent->RequestRenderRefresh();
	}
}

void UOptimizedSkeletalMeshWorldSubsystem::TickAnimation(const float DeltaTime)
{
	FOptimizedSkeletalMeshAnimationStats NewStats;
	NewStats.RegisteredInstances = Instances.Num();
	NewStats.LastDeltaTime = DeltaTime;

	for (TPair<int32, FOptimizedSkeletalMeshInstanceDesc>& Pair : Instances)
	{
		FOptimizedSkeletalMeshInstanceDesc& Desc = Pair.Value;
		if (!Desc.Animation)
		{
			InstanceBonePalettes.Remove(Pair.Key);
			continue;
		}

		++NewStats.AnimatedInstances;

		if (DeltaTime > 0.0f && Desc.bPlayAnimation && !FMath::IsNearlyZero(Desc.AnimationPlayRate))
		{
			const float SequenceLength = Desc.Animation->GetPlayLength();
			if (SequenceLength > 0.0f)
			{
				const float PreviousTime = Desc.AnimationTime;
				const float AdvancedTime = PreviousTime + DeltaTime * Desc.AnimationPlayRate;

				if (Desc.bLoopAnimation)
				{
					Desc.AnimationTime = WrapAnimationTime(AdvancedTime, SequenceLength);
				}
				else
				{
					Desc.AnimationTime = FMath::Clamp(AdvancedTime, 0.0f, SequenceLength);
					if (Desc.AnimationTime <= 0.0f || Desc.AnimationTime >= SequenceLength)
					{
						Desc.bPlayAnimation = false;
						++NewStats.FinishedInstances;
					}
				}

				if (!FMath::IsNearlyEqual(PreviousTime, Desc.AnimationTime))
				{
					++NewStats.AdvancedInstances;
				}
			}
		}

		TArray<FMatrix44f>& BonePalette = InstanceBonePalettes.FindOrAdd(Pair.Key);
		if (EvaluateInstanceBonePalette(Desc, BonePalette))
		{
			++NewStats.PoseEvaluatedInstances;
			++NewStats.BonePaletteInstances;
			NewStats.TotalBoneMatrices += BonePalette.Num();
			NewStats.MaxBonesPerInstance = FMath::Max(NewStats.MaxBonesPerInstance, BonePalette.Num());
		}
		else
		{
			InstanceBonePalettes.Remove(Pair.Key);
			++NewStats.FailedPoseEvaluations;
		}
	}

	LastAnimationStats = NewStats;
	OptimizedSkeletalMesh::PublishAnimationStats(LastAnimationStats);
}

FOptimizedSkeletalMeshAnimationMeshCache* UOptimizedSkeletalMeshWorldSubsystem::FindOrBuildAnimationMeshCache(
	USkeletalMesh* SkeletalMesh)
{
	if (!SkeletalMesh)
	{
		return nullptr;
	}

	const TObjectKey<USkeletalMesh> MeshKey(SkeletalMesh);
	if (FOptimizedSkeletalMeshAnimationMeshCache* ExistingCache = AnimationMeshCaches.Find(MeshKey))
	{
		return ExistingCache;
	}

	const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
	const int32 BoneCount = RefSkeleton.GetNum();
	if (BoneCount <= 0)
	{
		return nullptr;
	}

	FOptimizedSkeletalMeshAnimationMeshCache& NewCache = AnimationMeshCaches.Add(MeshKey);
	NewCache.RequiredBoneIndices.Reset(BoneCount);
	NewCache.RequiredBoneIndices.Reserve(BoneCount);
	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		NewCache.RequiredBoneIndices.Add(static_cast<FBoneIndexType>(BoneIndex));
	}

	NewCache.RequiredBones.InitializeTo(
		NewCache.RequiredBoneIndices,
		UE::Anim::FCurveFilterSettings(UE::Anim::ECurveFilterMode::DisallowAll),
		*SkeletalMesh);

	TArray<FTransform> RefComponentSpaceTransforms;
	RefComponentSpaceTransforms.SetNum(BoneCount);
	const TArray<FTransform>& RefLocalTransforms = RefSkeleton.GetRefBonePose();
	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
		if (ParentIndex != INDEX_NONE)
		{
			RefComponentSpaceTransforms[BoneIndex] = RefLocalTransforms[BoneIndex] * RefComponentSpaceTransforms[ParentIndex];
		}
		else
		{
			RefComponentSpaceTransforms[BoneIndex] = RefLocalTransforms[BoneIndex];
		}
	}

	NewCache.RefBasesInvMatrices.SetNum(BoneCount);
	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		NewCache.RefBasesInvMatrices[BoneIndex] = FMatrix44f(RefComponentSpaceTransforms[BoneIndex].ToMatrixWithScale().Inverse());
	}

	return &NewCache;
}

bool UOptimizedSkeletalMeshWorldSubsystem::EvaluateInstanceBonePalette(
	const FOptimizedSkeletalMeshInstanceDesc& Desc,
	TArray<FMatrix44f>& OutBonePalette)
{
	if (!Desc.SkeletalMesh || !Desc.Animation)
	{
		OutBonePalette.Reset();
		return false;
	}

	FOptimizedSkeletalMeshAnimationMeshCache* MeshCache = FindOrBuildAnimationMeshCache(Desc.SkeletalMesh);
	if (!MeshCache || MeshCache->RequiredBoneIndices.IsEmpty())
	{
		OutBonePalette.Reset();
		return false;
	}

	FCompactPose LocalPose;
	LocalPose.ResetToRefPose(MeshCache->RequiredBones);

	FBlendedCurve Curve;
	Curve.InitFrom(MeshCache->RequiredBones);

	UE::Anim::FStackAttributeContainer Attributes;
	FAnimationPoseData AnimationPoseData(LocalPose, Curve, Attributes);

	const FAnimExtractContext ExtractionContext(static_cast<double>(Desc.AnimationTime), false, FDeltaTimeRecord(0.0f), Desc.bLoopAnimation);
	Desc.Animation->GetBonePose(AnimationPoseData, ExtractionContext);

	const int32 BoneCount = MeshCache->RequiredBones.GetNumBones();
	TArray<FTransform> ComponentSpaceTransforms;
	ComponentSpaceTransforms.SetNum(BoneCount);

	for (const FCompactPoseBoneIndex CompactBoneIndex : LocalPose.ForEachBoneIndex())
	{
		const FMeshPoseBoneIndex MeshBoneIndex = MeshCache->RequiredBones.MakeMeshPoseIndex(CompactBoneIndex);
		const int32 BoneIndex = MeshBoneIndex.GetInt();
		if (!ComponentSpaceTransforms.IsValidIndex(BoneIndex))
		{
			continue;
		}

		const FCompactPoseBoneIndex ParentCompactBoneIndex = LocalPose.GetParentBoneIndex(CompactBoneIndex);
		if (ParentCompactBoneIndex != INDEX_NONE)
		{
			const FMeshPoseBoneIndex ParentMeshBoneIndex = MeshCache->RequiredBones.MakeMeshPoseIndex(ParentCompactBoneIndex);
			ComponentSpaceTransforms[BoneIndex] = LocalPose[CompactBoneIndex] * ComponentSpaceTransforms[ParentMeshBoneIndex.GetInt()];
		}
		else
		{
			ComponentSpaceTransforms[BoneIndex] = LocalPose[CompactBoneIndex];
		}
	}

	OutBonePalette.SetNum(BoneCount);
	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		const FMatrix44f ComponentSpaceMatrix(ComponentSpaceTransforms[BoneIndex].ToMatrixWithScale());
		OutBonePalette[BoneIndex] = MeshCache->RefBasesInvMatrices[BoneIndex] * ComponentSpaceMatrix;
	}

	return true;
}

float UOptimizedSkeletalMeshWorldSubsystem::WrapAnimationTime(
	const float AnimationTime,
	const float SequenceLength)
{
	if (SequenceLength <= 0.0f)
	{
		return 0.0f;
	}

	float WrappedTime = FMath::Fmod(AnimationTime, SequenceLength);
	if (WrappedTime < 0.0f)
	{
		WrappedTime += SequenceLength;
	}

	return WrappedTime;
}
