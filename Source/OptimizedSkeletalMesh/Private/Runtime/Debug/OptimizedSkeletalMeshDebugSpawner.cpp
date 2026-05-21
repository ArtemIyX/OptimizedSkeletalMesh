// Copyright Epic Games, Inc. All Rights Reserved.

#include "Runtime/Debug/OptimizedSkeletalMeshDebugSpawner.h"

#include "Engine/World.h"
#include "Runtime/Rendering/OptimizedSkeletalMeshRenderComponent.h"

namespace OptimizedSkeletalMesh::Debug
{
	static bool CanUseSpawnerWorld(const UWorld* InWorld)
	{
		return InWorld && (InWorld->IsGameWorld() || InWorld->WorldType == EWorldType::Editor);
	}
} // namespace OptimizedSkeletalMesh::Debug

AOptimizedSkeletalMeshDebugSpawner::AOptimizedSkeletalMeshDebugSpawner()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	SetCanBeDamaged(false);
	SetActorEnableCollision(false);

	PreviewRenderComponent = CreateDefaultSubobject<UOptimizedSkeletalMeshRenderComponent>(
		TEXT("OptimizedSkeletalMeshPreviewRenderComponent"));
	RootComponent = PreviewRenderComponent;
}

void AOptimizedSkeletalMeshDebugSpawner::OnConstruction(const FTransform& InTransform)
{
	Super::OnConstruction(InTransform);

	const UWorld* world = GetWorld();
	if (bRebuildOnConstruction && world && world->WorldType == EWorldType::Editor)
	{
		RebuildInstances();
	}
}

void AOptimizedSkeletalMeshDebugSpawner::Tick(const float InDeltaSeconds)
{
	Super::Tick(InDeltaSeconds);

	if (UOptimizedSkeletalMeshWorldSubsystem* subsystem = GetOptimizedSubsystem())
	{
		if (PreviewRenderComponent && subsystem->HasDirtyBonePalettes() && PreviewRenderComponent->PushBonePalettesToRenderThread())
		{
			subsystem->ClearDirtyBonePalettes();
		}

		LastAnimationStats = subsystem->GetLastAnimationStats();
	}

	if (PreviewRenderComponent)
	{
		LastRenderStats = PreviewRenderComponent->GetLastRenderStats();
	}
}

void AOptimizedSkeletalMeshDebugSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (UOptimizedSkeletalMeshWorldSubsystem* subsystem = GetOptimizedSubsystem())
	{
		subsystem->RegisterExternalRenderComponent(PreviewRenderComponent);
	}

	RebuildInstances();
}

void AOptimizedSkeletalMeshDebugSpawner::EndPlay(const EEndPlayReason::Type InEndPlayReason)
{
	if (UOptimizedSkeletalMeshWorldSubsystem* subsystem = GetOptimizedSubsystem())
	{
		subsystem->UnregisterExternalRenderComponent(PreviewRenderComponent);
	}

	ClearInstances();

	Super::EndPlay(InEndPlayReason);
}

void AOptimizedSkeletalMeshDebugSpawner::RebuildInstances()
{
	UOptimizedSkeletalMeshWorldSubsystem* subsystem = GetOptimizedSubsystem();
	if (!subsystem)
	{
		return;
	}

	subsystem->SetExternalRenderBridgeActive(PreviewRenderComponent != nullptr);

	ClearInstances();

	if (!SkeletalMesh)
	{
		return;
	}

	subsystem->SetExternalRenderBridgeActive(PreviewRenderComponent != nullptr);

	FOptimizedSkeletalMeshRenderSettings renderSettings;
	renderSettings.bDrawDebugBounds = bDrawDebugBounds;
	renderSettings.bDrawMeshSections = bDrawMeshSections;
	renderSettings.MeshDrawMode = MeshDrawMode;
	renderSettings.bEnableInstanceFrustumCulling = bEnableInstanceFrustumCulling;
	renderSettings.InstanceCullBoundsScale = InstanceCullBoundsScale;
	renderSettings.bUseConservativeProxyBounds = bUseConservativeProxyBounds;
	renderSettings.ConservativeProxyBoundsExtent = ConservativeProxyBoundsExtent;
	renderSettings.bDrawCullingDebug = bDrawCullingDebug;
	renderSettings.bDrawCullTestBounds = bDrawCullTestBounds;
	subsystem->ApplyRenderSettings(renderSettings);

	if (PreviewRenderComponent)
	{
		PreviewRenderComponent->SetOptimizedSkeletalMeshSubsystem(subsystem);
		subsystem->RegisterExternalRenderComponent(PreviewRenderComponent);
		subsystem->ApplyRenderSettingsToComponent(PreviewRenderComponent);
	}

	SpawnedHandles.Reserve(CountX * CountY);

	for (int32 y = 0; y < CountY; ++y)
	{
		for (int32 x = 0; x < CountX; ++x)
		{
			FOptimizedSkeletalMeshInstanceDesc desc;
			desc.SkeletalMesh = SkeletalMesh;
			desc.WorldTransform = GetInstanceTransform(x, y);
			desc.LODIndex = ForcedLODIndex;
			desc.bAutoLOD = bAutoLOD;
			desc.Animation = Animation;
			desc.AnimationTime = AnimationStartTime;
			desc.AnimationPlayRate = AnimationPlayRate;
			desc.bLoopAnimation = bLoopAnimation;
			desc.bPlayAnimation = bPlayAnimation;
			desc.AnimationUpdateRateHz = AnimationUpdateRateHz;
			desc.bVisible = true;

			const FOptimizedSkeletalMeshInstanceHandle handle = subsystem->RegisterInstance(desc);
			if (handle.IsValid())
			{
				SpawnedHandles.Add(handle);
			}
		}
	}

	SpawnedInstanceCount = SpawnedHandles.Num();
	VisibleRenderBatchCount = subsystem->GetVisibleRenderBatchCount();

	if (PreviewRenderComponent)
	{
		PreviewRenderComponent->RequestRenderRefresh();
	}
}

void AOptimizedSkeletalMeshDebugSpawner::ClearInstances()
{
	if (UOptimizedSkeletalMeshWorldSubsystem* subsystem = GetOptimizedSubsystem())
	{
		const bool bUsePreviewRenderBridge = PreviewRenderComponent != nullptr;
		subsystem->SetExternalRenderBridgeActive(bUsePreviewRenderBridge);

		for (const FOptimizedSkeletalMeshInstanceHandle& handle : SpawnedHandles)
		{
			subsystem->UnregisterInstance(handle);
		}
	}

	SpawnedHandles.Reset();
	SpawnedInstanceCount = 0;
	VisibleRenderBatchCount = 0;
	LastRenderStats = FOptimizedSkeletalMeshRenderStats();
	LastAnimationStats = FOptimizedSkeletalMeshAnimationStats();

	if (PreviewRenderComponent)
	{
		PreviewRenderComponent->RequestRenderRefresh();
	}
}

UOptimizedSkeletalMeshWorldSubsystem* AOptimizedSkeletalMeshDebugSpawner::GetOptimizedSubsystem() const
{
	const UWorld* world = GetWorld();
	return OptimizedSkeletalMesh::Debug::CanUseSpawnerWorld(world)
		? world->GetSubsystem<UOptimizedSkeletalMeshWorldSubsystem>()
		: nullptr;
}

FTransform AOptimizedSkeletalMeshDebugSpawner::GetInstanceTransform(const int32 InX, const int32 InY) const
{
	const FVector centerOffset(
		(static_cast<float>(CountX - 1) * Spacing) * 0.5f,
		(static_cast<float>(CountY - 1) * Spacing) * 0.5f,
		0.0f);

	const FVector localOffset(
		static_cast<float>(InX) * Spacing,
		static_cast<float>(InY) * Spacing,
		0.0f);

	const FTransform actorTransform = GetActorTransform();
	FTransform instanceTransform = actorTransform;
	instanceTransform.SetLocation(actorTransform.TransformPosition(localOffset - centerOffset));

	return instanceTransform;
}
