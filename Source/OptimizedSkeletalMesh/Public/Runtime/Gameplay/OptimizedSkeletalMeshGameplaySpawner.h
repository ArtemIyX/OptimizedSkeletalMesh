// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Runtime/Manager/OptimizedSkeletalMeshWorldSubsystem.h"
#include "OptimizedSkeletalMeshGameplaySpawner.generated.h"

class UAnimSequence;
class USkeletalMesh;

UCLASS(Blueprintable)
class OPTIMIZEDSKELETALMESH_API AOptimizedSkeletalMeshGameplaySpawner : public AActor
{
	GENERATED_BODY()

public:
#pragma region Lifecycle
	AOptimizedSkeletalMeshGameplaySpawner();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type InEndPlayReason) override;
#pragma endregion

private:
#pragma region Settings
	UPROPERTY(EditAnywhere, Category = "Optimized Skeletal Mesh")
	TObjectPtr<USkeletalMesh> Mesh = nullptr;

	UPROPERTY(EditAnywhere, Category = "Optimized Skeletal Mesh")
	TObjectPtr<UAnimSequence> Anim = nullptr;

	UPROPERTY(EditAnywhere, Category = "Optimized Skeletal Mesh")
	bool bLoopAnim = true;

	UPROPERTY(EditAnywhere, Category = "Optimized Skeletal Mesh", meta = (ClampMin = "1"))
	int32 CountX = 25;

	UPROPERTY(EditAnywhere, Category = "Optimized Skeletal Mesh", meta = (ClampMin = "1"))
	int32 CountY = 20;

	UPROPERTY(EditAnywhere, Category = "Optimized Skeletal Mesh", meta = (ClampMin = "1.0"))
	float Spacing = 150.0f;
#pragma endregion

#pragma region Runtime
	UPROPERTY(Transient)
	TArray<FOptimizedSkeletalMeshInstanceHandle> SpawnedHandles;
#pragma endregion

#pragma region Helpers
	UOptimizedSkeletalMeshWorldSubsystem* GetSubsystem() const;
	void RebuildInstances();
	void ClearInstances();
	FTransform MakeInstanceTransform(int32 InX, int32 InY) const;
#pragma endregion
};

