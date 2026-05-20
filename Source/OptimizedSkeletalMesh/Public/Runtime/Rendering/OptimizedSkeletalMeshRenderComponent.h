// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "OptimizedSkeletalMeshRenderComponent.generated.h"

class FPrimitiveSceneProxy;
class UOptimizedSkeletalMeshWorldSubsystem;

UCLASS(NotBlueprintable, ClassGroup = Rendering)
class OPTIMIZEDSKELETALMESH_API UOptimizedSkeletalMeshRenderComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UOptimizedSkeletalMeshRenderComponent(const FObjectInitializer& ObjectInitializer);

	void SetOptimizedSkeletalMeshSubsystem(UOptimizedSkeletalMeshWorldSubsystem* InSubsystem);
	void RequestRenderRefresh();

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual bool ShouldRecreateProxyOnUpdateTransform() const override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UOptimizedSkeletalMeshWorldSubsystem> Subsystem = nullptr;
};
