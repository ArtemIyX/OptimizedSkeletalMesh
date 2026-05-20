// Copyright Epic Games, Inc. All Rights Reserved.

#include "Runtime/Manager/OptimizedSkeletalMeshWorldSubsystem.h"

#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Runtime/Rendering/OptimizedSkeletalMeshRenderComponent.h"
#include "Stats/Stats.h"

void UOptimizedSkeletalMeshWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Instances.Reset();
	FreeInstanceIds.Reset();
	NextInstanceId = 1;
	bRenderDataDirty = false;
	LastAnimationStats = FOptimizedSkeletalMeshAnimationStats();
	EnsureRenderBridge();
}

void UOptimizedSkeletalMeshWorldSubsystem::Deinitialize()
{
	DestroyRenderBridge();

	Instances.Reset();
	FreeInstanceIds.Reset();
	NextInstanceId = 1;
	bRenderDataDirty = false;
	LastAnimationStats = FOptimizedSkeletalMeshAnimationStats();

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

	if (DeltaTime <= 0.0f)
	{
		LastAnimationStats = NewStats;
		return;
	}

	for (TPair<int32, FOptimizedSkeletalMeshInstanceDesc>& Pair : Instances)
	{
		FOptimizedSkeletalMeshInstanceDesc& Desc = Pair.Value;
		if (!Desc.Animation)
		{
			continue;
		}

		++NewStats.AnimatedInstances;

		if (!Desc.bPlayAnimation || FMath::IsNearlyZero(Desc.AnimationPlayRate))
		{
			continue;
		}

		const float SequenceLength = Desc.Animation->GetPlayLength();
		if (SequenceLength <= 0.0f)
		{
			continue;
		}

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

	LastAnimationStats = NewStats;
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
