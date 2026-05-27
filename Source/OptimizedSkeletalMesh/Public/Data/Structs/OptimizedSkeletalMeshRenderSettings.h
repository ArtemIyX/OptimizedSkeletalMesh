// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Rendering/OptimizedSkeletalMeshRenderComponent.h"
#include "OptimizedSkeletalMeshRenderSettings.generated.h"

USTRUCT(BlueprintType)
struct OPTIMIZEDSKELETALMESH_API FOptimizedSkeletalMeshRenderSettings
{
	GENERATED_BODY()
public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	bool bDrawDebugBounds = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	bool bDrawMeshSections = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	EOptimizedSkeletalMeshDrawMode MeshDrawMode = EOptimizedSkeletalMeshDrawMode::GpuSkinnedInstanced;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	bool bEnableInstanceFrustumCulling = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "1.0"))
	float InstanceCullBoundsScale = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	bool bUseConservativeProxyBounds = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "1000.0"))
	float ConservativeProxyBoundsExtent = 10000000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	bool bDrawCullingDebug = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	bool bDrawCullTestBounds = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering")
	bool bCastShadows = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0.0"))
	float NearFullShadowDistance = 1800.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0.0"))
	float MidShadowDistance = 3200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "1"))
	int32 MidShadowUpdateDivisor = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0"))
	int32 FarShadowUpdateDivisor = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0.0"))
	float MaxShadowCastDistance = 5000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0"))
	int32 MaxDynamicShadowCasters = 120;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0"))
	int32 NearShadowLodBias = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0"))
	int32 MidShadowLodBias = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0"))
	int32 FarShadowLodBias = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0"))
	int32 MaxShadowSectionsPerLOD = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0.0"))
	float LocalLightMaxShadowCastDistance = 2000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0"))
	int32 LocalLightMaxDynamicShadowCasters = 24;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0"))
	int32 LocalLightShadowLodBias = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optimized Skeletal Mesh|Rendering", meta = (ClampMin = "0"))
	int32 LocalLightMaxShadowSectionsPerLOD = 1;
};