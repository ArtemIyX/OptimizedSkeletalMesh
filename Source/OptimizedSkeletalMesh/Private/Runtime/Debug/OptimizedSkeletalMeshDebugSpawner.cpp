// Copyright Epic Games, Inc. All Rights Reserved.

#include "Runtime/Debug/OptimizedSkeletalMeshDebugSpawner.h"

#include "Engine/World.h"
#include "Runtime/Rendering/OptimizedSkeletalMeshRenderComponent.h"

namespace OptimizedSkeletalMesh::Debug
{
	static bool CanUseSpawnerWorld(const UWorld* World)
	{
		return World && (World->IsGameWorld() || World->WorldType == EWorldType::Editor);
	}
}

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

void AOptimizedSkeletalMeshDebugSpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	const UWorld* World = GetWorld();
	if (bRebuildOnConstruction && World && World->WorldType == EWorldType::Editor)
	{
		RebuildInstances();
	}
}

void AOptimizedSkeletalMeshDebugSpawner::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (PreviewRenderComponent)
	{
		LastRenderStats = PreviewRenderComponent->GetLastRenderStats();
	}

	if (const UOptimizedSkeletalMeshWorldSubsystem* Subsystem = GetOptimizedSubsystem())
	{
		LastAnimationStats = Subsystem->GetLastAnimationStats();
	}
}

void AOptimizedSkeletalMeshDebugSpawner::BeginPlay()
{
	Super::BeginPlay();

	RebuildInstances();
}

void AOptimizedSkeletalMeshDebugSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearInstances();

	Super::EndPlay(EndPlayReason);
}

void AOptimizedSkeletalMeshDebugSpawner::RebuildInstances()
{
	UOptimizedSkeletalMeshWorldSubsystem* Subsystem = GetOptimizedSubsystem();
	if (!Subsystem)
	{
		return;
	}

	Subsystem->SetExternalRenderBridgeActive(PreviewRenderComponent != nullptr);

	ClearInstances();

	if (!SkeletalMesh)
	{
		return;
	}

	Subsystem->SetExternalRenderBridgeActive(PreviewRenderComponent != nullptr);

	if (PreviewRenderComponent)
	{
		PreviewRenderComponent->SetOptimizedSkeletalMeshSubsystem(Subsystem);
		PreviewRenderComponent->SetDrawDebugBounds(bDrawDebugBounds);
		PreviewRenderComponent->SetDrawMeshSections(bDrawMeshSections);
		PreviewRenderComponent->SetMeshDrawMode(MeshDrawMode);
		PreviewRenderComponent->SetMaxMeshDrawInstances(MaxMeshDrawInstances);
		PreviewRenderComponent->SetInstanceFrustumCulling(bEnableInstanceFrustumCulling);
		PreviewRenderComponent->SetInstanceCullBoundsScale(InstanceCullBoundsScale);
		PreviewRenderComponent->SetConservativeProxyBounds(bUseConservativeProxyBounds);
		PreviewRenderComponent->SetConservativeProxyBoundsExtent(ConservativeProxyBoundsExtent);
		PreviewRenderComponent->SetDrawCullingDebug(bDrawCullingDebug);
		PreviewRenderComponent->SetDrawCullTestBounds(bDrawCullTestBounds);
	}

	SpawnedHandles.Reserve(CountX * CountY);

	for (int32 Y = 0; Y < CountY; ++Y)
	{
		for (int32 X = 0; X < CountX; ++X)
		{
			FOptimizedSkeletalMeshInstanceDesc Desc;
			Desc.SkeletalMesh = SkeletalMesh;
			Desc.WorldTransform = GetInstanceTransform(X, Y);
			Desc.LODIndex = ForcedLODIndex;
			Desc.bAutoLOD = bAutoLOD;
			Desc.Animation = Animation;
			Desc.AnimationTime = AnimationStartTime;
			Desc.AnimationPlayRate = AnimationPlayRate;
			Desc.bLoopAnimation = bLoopAnimation;
			Desc.bPlayAnimation = bPlayAnimation;
			Desc.bVisible = true;

			const FOptimizedSkeletalMeshInstanceHandle Handle = Subsystem->RegisterInstance(Desc);
			if (Handle.IsValid())
			{
				SpawnedHandles.Add(Handle);
			}
		}
	}

	SpawnedInstanceCount = SpawnedHandles.Num();
	VisibleRenderBatchCount = Subsystem->GetVisibleRenderBatchCount();

	if (PreviewRenderComponent)
	{
		PreviewRenderComponent->RequestRenderRefresh();
	}
}

void AOptimizedSkeletalMeshDebugSpawner::ClearInstances()
{
	if (UOptimizedSkeletalMeshWorldSubsystem* Subsystem = GetOptimizedSubsystem())
	{
		const bool bUsePreviewRenderBridge = PreviewRenderComponent != nullptr;
		Subsystem->SetExternalRenderBridgeActive(bUsePreviewRenderBridge);

		for (const FOptimizedSkeletalMeshInstanceHandle& Handle : SpawnedHandles)
		{
			Subsystem->UnregisterInstance(Handle);
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
	const UWorld* World = GetWorld();
	return OptimizedSkeletalMesh::Debug::CanUseSpawnerWorld(World)
		? World->GetSubsystem<UOptimizedSkeletalMeshWorldSubsystem>()
		: nullptr;
}

FTransform AOptimizedSkeletalMeshDebugSpawner::GetInstanceTransform(const int32 X, const int32 Y) const
{
	const FVector CenterOffset(
		(static_cast<float>(CountX - 1) * Spacing) * 0.5f,
		(static_cast<float>(CountY - 1) * Spacing) * 0.5f,
		0.0f);

	const FVector LocalOffset(
		static_cast<float>(X) * Spacing,
		static_cast<float>(Y) * Spacing,
		0.0f);

	const FTransform ActorTransform = GetActorTransform();
	FTransform InstanceTransform = ActorTransform;
	InstanceTransform.SetLocation(ActorTransform.TransformPosition(LocalOffset - CenterOffset));

	return InstanceTransform;
}
