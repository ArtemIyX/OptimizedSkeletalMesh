// Copyright Epic Games, Inc. All Rights Reserved.

#include "Runtime/Rendering/OptimizedSkeletalMeshRenderBridgeActor.h"

#include "Components/SceneComponent.h"
#include "Runtime/Rendering/OptimizedSkeletalMeshRenderComponent.h"

AOptimizedSkeletalMeshRenderBridgeActor::AOptimizedSkeletalMeshRenderBridgeActor()
{
	SetCanBeDamaged(false);
	SetActorEnableCollision(false);
	SetActorHiddenInGame(false);
	SetHidden(false);
	
	USceneComponent* root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	root->SetMobility(EComponentMobility::Movable);
	RootComponent = root;

	RenderComponent = CreateDefaultSubobject<UOptimizedSkeletalMeshRenderComponent>(TEXT("OptimizedSkeletalMeshRenderComponent"));
	RenderComponent->SetupAttachment(RootComponent);
	RenderComponent->SetHiddenInGame(false, true);
	RenderComponent->SetVisibility(true, true);
	RenderComponent->SetMobility(EComponentMobility::Movable);
	RenderComponent->SetCastShadow(true);
	
}

