// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OptimizedSkeletalMeshRenderBridgeActor.generated.h"

class UOptimizedSkeletalMeshRenderComponent;

UCLASS(NotBlueprintable, Transient)
class OPTIMIZEDSKELETALMESH_API AOptimizedSkeletalMeshRenderBridgeActor : public AActor
{
	GENERATED_BODY()

public:
	AOptimizedSkeletalMeshRenderBridgeActor();

	UOptimizedSkeletalMeshRenderComponent* GetRenderComponent() const { return RenderComponent; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Optimized Skeletal Mesh")
	TObjectPtr<UOptimizedSkeletalMeshRenderComponent> RenderComponent = nullptr;
};

