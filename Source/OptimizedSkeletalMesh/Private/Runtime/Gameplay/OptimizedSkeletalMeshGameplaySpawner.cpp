// Copyright Epic Games, Inc. All Rights Reserved.

#include "Runtime/Gameplay/OptimizedSkeletalMeshGameplaySpawner.h"

#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "Logging/LogMacros.h"

AOptimizedSkeletalMeshGameplaySpawner::AOptimizedSkeletalMeshGameplaySpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);
	SetActorEnableCollision(false);
}

void AOptimizedSkeletalMeshGameplaySpawner::BeginPlay()
{
	Super::BeginPlay();
	RebuildInstances();
	const UWorld* world = GetWorld();
	if (UOptimizedSkeletalMeshWorldSubsystem* subsystem = GetSubsystem())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("OSM Spawner BeginPlay: world=%s type=%d netmode=%d instances=%d mesh=%s anim=%s"),
			*GetNameSafe(world),
			world ? static_cast<int32>(world->WorldType) : -1,
			world ? static_cast<int32>(world->GetNetMode()) : -1,
			subsystem->GetInstanceCount(),
			*GetNameSafe(Mesh),
			*GetNameSafe(Anim));
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("OSM Spawner BeginPlay: skipped world=%s type=%d netmode=%d"),
			*GetNameSafe(world),
			world ? static_cast<int32>(world->WorldType) : -1,
			world ? static_cast<int32>(world->GetNetMode()) : -1);
	}
}

void AOptimizedSkeletalMeshGameplaySpawner::EndPlay(const EEndPlayReason::Type InEndPlayReason)
{
	ClearInstances();
	Super::EndPlay(InEndPlayReason);
}

UOptimizedSkeletalMeshWorldSubsystem* AOptimizedSkeletalMeshGameplaySpawner::GetSubsystem() const
{
	const UWorld* world = GetWorld();
	return world && world->IsGameWorld() && world->GetNetMode() != NM_DedicatedServer
		? world->GetSubsystem<UOptimizedSkeletalMeshWorldSubsystem>()
		: nullptr;
}

void AOptimizedSkeletalMeshGameplaySpawner::RebuildInstances()
{
	UOptimizedSkeletalMeshWorldSubsystem* subsystem = GetSubsystem();
	if (!subsystem || !Mesh)
	{
		return;
	}

	subsystem->SetExternalRenderBridgeActive(false);
	ClearInstances();

	/*FOptimizedSkeletalMeshRenderSettings renderSettings = subsystem->GetRenderSettings();
	renderSettings.bDrawMeshSections = true;
	renderSettings.MeshDrawMode = EOptimizedSkeletalMeshDrawMode::DynamicMeshProof;
	subsystem->ApplyRenderSettings(renderSettings);*/

	TArray<FTransform> worldTransforms;
	worldTransforms.Reserve(CountX * CountY);
	for (int32 y = 0; y < CountY; ++y)
	{
		for (int32 x = 0; x < CountX; ++x)
		{
			worldTransforms.Add(MakeInstanceTransform(x, y));
		}
	}

	FOptimizedSkeletalMeshInstanceDesc baseDesc;
	baseDesc.SkeletalMesh = Mesh;
	baseDesc.Animation = Anim;
	baseDesc.bLoopAnimation = bLoopAnim;
	baseDesc.bPlayAnimation = Anim != nullptr;
	baseDesc.AnimationPlayRate = 1.0f;
	baseDesc.AnimationTime = 0.0f;
	baseDesc.bVisible = true;

	subsystem->AddInstancesBatch(baseDesc, worldTransforms, SpawnedHandles);
}

void AOptimizedSkeletalMeshGameplaySpawner::ClearInstances()
{
	UOptimizedSkeletalMeshWorldSubsystem* subsystem = GetSubsystem();
	if (!subsystem || SpawnedHandles.IsEmpty())
	{
		SpawnedHandles.Reset();
		return;
	}

	subsystem->RemoveInstances(SpawnedHandles);
	SpawnedHandles.Reset();
}

FTransform AOptimizedSkeletalMeshGameplaySpawner::MakeInstanceTransform(const int32 InX, const int32 InY) const
{
	const FVector centerOffset(
		(static_cast<float>(CountX - 1) * Spacing) * 0.5f,
		(static_cast<float>(CountY - 1) * Spacing) * 0.5f,
		0.0f);
	const FVector localOffset(static_cast<float>(InX) * Spacing, static_cast<float>(InY) * Spacing, 0.0f);
	const FTransform actorTransform = GetActorTransform();
	FTransform instanceTransform = actorTransform;
	instanceTransform.SetLocation(actorTransform.TransformPosition(localOffset - centerOffset));
	return instanceTransform;
}
