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
	void SetDrawDebugBounds(bool bInDrawDebugBounds);
	void SetDrawMeshSections(bool bInDrawMeshSections);
	void SetMaxMeshDrawInstances(int32 InMaxMeshDrawInstances);
	void RequestRenderRefresh();

	bool ShouldDrawDebugBounds() const { return bDrawDebugBounds; }
	bool ShouldDrawMeshSections() const { return bDrawMeshSections; }
	int32 GetMaxMeshDrawInstances() const { return MaxMeshDrawInstances; }

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual bool ShouldRecreateProxyOnUpdateTransform() const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;

private:
	UPROPERTY(EditAnywhere, Category = "Optimized Skeletal Mesh|Debug")
	bool bDrawDebugBounds = true;

	UPROPERTY(EditAnywhere, Category = "Optimized Skeletal Mesh|Debug")
	bool bDrawMeshSections = false;

	UPROPERTY(EditAnywhere, Category = "Optimized Skeletal Mesh|Debug", meta = (ClampMin = "0"))
	int32 MaxMeshDrawInstances = 1;

	UPROPERTY(Transient)
	TObjectPtr<UOptimizedSkeletalMeshWorldSubsystem> Subsystem = nullptr;
};
