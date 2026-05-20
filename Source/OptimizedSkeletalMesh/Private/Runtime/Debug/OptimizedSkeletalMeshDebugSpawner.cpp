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
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);
	SetActorEnableCollision(false);

	PreviewRenderComponent = CreateDefaultSubobject<UOptimizedSkeletalMeshRenderComponent>(
		TEXT("OptimizedSkeletalMeshPreviewRenderComponent"));
	RootComponent = PreviewRenderComponent;
}

void AOptimizedSkeletalMeshDebugSpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bRebuildOnConstruction)
	{
		RebuildInstances();
	}
}

void AOptimizedSkeletalMeshDebugSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearInstances();

	Super::EndPlay(EndPlayReason);
}

void AOptimizedSkeletalMeshDebugSpawner::RebuildInstances()
{
	ClearInstances();

	if (!SkeletalMesh)
	{
		return;
	}

	UOptimizedSkeletalMeshWorldSubsystem* Subsystem = GetOptimizedSubsystem();
	if (!Subsystem)
	{
		return;
	}

	if (PreviewRenderComponent)
	{
		PreviewRenderComponent->SetOptimizedSkeletalMeshSubsystem(Subsystem);
		PreviewRenderComponent->SetDrawDebugBounds(bDrawDebugBounds);
		PreviewRenderComponent->SetDrawMeshSections(bDrawMeshSections);
		PreviewRenderComponent->SetMeshDrawMode(MeshDrawMode);
		PreviewRenderComponent->SetMaxMeshDrawInstances(MaxMeshDrawInstances);
	}

	SpawnedHandles.Reserve(CountX * CountY);

	for (int32 Y = 0; Y < CountY; ++Y)
	{
		for (int32 X = 0; X < CountX; ++X)
		{
			FOptimizedSkeletalMeshInstanceDesc Desc;
			Desc.SkeletalMesh = SkeletalMesh;
			Desc.WorldTransform = GetInstanceTransform(X, Y);
			Desc.LODIndex = 0;
			Desc.AnimationTime = 0.0f;
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
		for (const FOptimizedSkeletalMeshInstanceHandle& Handle : SpawnedHandles)
		{
			Subsystem->UnregisterInstance(Handle);
		}
	}

	SpawnedHandles.Reset();
	SpawnedInstanceCount = 0;
	VisibleRenderBatchCount = 0;

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

	FTransform InstanceTransform = GetActorTransform();
	InstanceTransform.AddToTranslation(LocalOffset - CenterOffset);

	return InstanceTransform;
}
