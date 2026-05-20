// Copyright Epic Games, Inc. All Rights Reserved.

#include "Runtime/Rendering/OptimizedSkeletalMeshRenderComponent.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "MeshElementCollector.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "Runtime/Manager/OptimizedSkeletalMeshWorldSubsystem.h"
#include "SceneManagement.h"

namespace OptimizedSkeletalMesh
{
	static constexpr float FallbackInstanceExtent = 50.0f;

	struct FDebugInstance
	{
		FBox WorldBounds;
	};

	struct FRenderBatchKey
	{
		TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;
		int32 LODIndex = 0;

		bool operator==(const FRenderBatchKey& Other) const
		{
			return SkeletalMesh == Other.SkeletalMesh && LODIndex == Other.LODIndex;
		}
	};

	uint32 GetTypeHash(const FRenderBatchKey& Key)
	{
		return HashCombine(PointerHash(Key.SkeletalMesh.Get()), ::GetTypeHash(Key.LODIndex));
	}

	struct FRenderBatch
	{
		FRenderBatchKey Key;
		TArray<FDebugInstance> Instances;
		FColor DebugColor = FColor::Yellow;
	};

	static FBox GetInstanceWorldBounds(const FOptimizedSkeletalMeshInstanceDesc& Desc)
	{
		if (Desc.SkeletalMesh)
		{
			return Desc.SkeletalMesh->GetBounds().GetBox().TransformBy(Desc.WorldTransform);
		}

		return FBox::BuildAABB(Desc.WorldTransform.GetLocation(), FVector(FallbackInstanceExtent));
	}

	static FColor GetBatchDebugColor(const int32 BatchIndex)
	{
		static const FColor Colors[] =
		{
			FColor::Yellow,
			FColor::Cyan,
			FColor::Green,
			FColor::Orange,
			FColor::Magenta,
			FColor::Red,
			FColor::Blue,
			FColor::White,
		};

		return Colors[BatchIndex % UE_ARRAY_COUNT(Colors)];
	}
}

class FOptimizedSkeletalMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	explicit FOptimizedSkeletalMeshSceneProxy(const UOptimizedSkeletalMeshRenderComponent* Component)
		: FPrimitiveSceneProxy(Component)
	{
		if (const UWorld* World = Component->GetWorld())
		{
			if (const UOptimizedSkeletalMeshWorldSubsystem* Subsystem = World->GetSubsystem<UOptimizedSkeletalMeshWorldSubsystem>())
			{
				TArray<FOptimizedSkeletalMeshInstanceSnapshot> Snapshots;
				Subsystem->GetInstancesSnapshot(Snapshots);

				TMap<OptimizedSkeletalMesh::FRenderBatchKey, int32> BatchIndexByKey;
				RenderBatches.Reserve(Snapshots.Num());

				for (const FOptimizedSkeletalMeshInstanceSnapshot& Snapshot : Snapshots)
				{
					if (!Snapshot.Desc.bVisible || !Snapshot.Desc.SkeletalMesh)
					{
						continue;
					}

					OptimizedSkeletalMesh::FRenderBatchKey BatchKey;
					BatchKey.SkeletalMesh = Snapshot.Desc.SkeletalMesh;
					BatchKey.LODIndex = FMath::Max(0, Snapshot.Desc.LODIndex);

					int32* ExistingBatchIndex = BatchIndexByKey.Find(BatchKey);
					if (!ExistingBatchIndex)
					{
						const int32 NewBatchIndex = RenderBatches.Num();
						OptimizedSkeletalMesh::FRenderBatch& NewBatch = RenderBatches.AddDefaulted_GetRef();
						NewBatch.Key = BatchKey;
						NewBatch.DebugColor = OptimizedSkeletalMesh::GetBatchDebugColor(NewBatchIndex);
						BatchIndexByKey.Add(BatchKey, NewBatchIndex);
						ExistingBatchIndex = BatchIndexByKey.Find(BatchKey);
					}

					OptimizedSkeletalMesh::FRenderBatch& Batch = RenderBatches[*ExistingBatchIndex];
					OptimizedSkeletalMesh::FDebugInstance& DebugInstance = Batch.Instances.AddDefaulted_GetRef();
					DebugInstance.WorldBounds = OptimizedSkeletalMesh::GetInstanceWorldBounds(Snapshot.Desc);
				}
			}
		}
	}

	virtual SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			if (!(VisibilityMap & (1 << ViewIndex)))
			{
				continue;
			}

			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
			for (const OptimizedSkeletalMesh::FRenderBatch& Batch : RenderBatches)
			{
				for (const OptimizedSkeletalMesh::FDebugInstance& DebugInstance : Batch.Instances)
				{
					DrawWireBox(PDI, DebugInstance.WorldBounds, Batch.DebugColor, SDPG_Foreground);
				}
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bDynamicRelevance = true;
		Result.bShadowRelevance = false;
		Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
		return Result;
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetOptimizedAllocatedSize();
	}

	uint32 GetOptimizedAllocatedSize() const
	{
		SIZE_T AllocatedSize = FPrimitiveSceneProxy::GetAllocatedSize() + RenderBatches.GetAllocatedSize();
		for (const OptimizedSkeletalMesh::FRenderBatch& Batch : RenderBatches)
		{
			AllocatedSize += Batch.Instances.GetAllocatedSize();
		}

		return IntCastChecked<uint32>(AllocatedSize);
	}

private:
	TArray<OptimizedSkeletalMesh::FRenderBatch> RenderBatches;
};

UOptimizedSkeletalMeshRenderComponent::UOptimizedSkeletalMeshRenderComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Mobility = EComponentMobility::Movable;
	BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	bCastDynamicShadow = false;
	CastShadow = false;
}

void UOptimizedSkeletalMeshRenderComponent::SetOptimizedSkeletalMeshSubsystem(
	UOptimizedSkeletalMeshWorldSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
	RequestRenderRefresh();
}

void UOptimizedSkeletalMeshRenderComponent::RequestRenderRefresh()
{
	UpdateBounds();
	MarkRenderStateDirty();
}

FPrimitiveSceneProxy* UOptimizedSkeletalMeshRenderComponent::CreateSceneProxy()
{
	return new FOptimizedSkeletalMeshSceneProxy(this);
}

FBoxSphereBounds UOptimizedSkeletalMeshRenderComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox WorldBox(ForceInit);

	if (Subsystem)
	{
		TArray<FOptimizedSkeletalMeshInstanceSnapshot> Snapshots;
		Subsystem->GetInstancesSnapshot(Snapshots);

		for (const FOptimizedSkeletalMeshInstanceSnapshot& Snapshot : Snapshots)
		{
			if (Snapshot.Desc.bVisible)
			{
				WorldBox += OptimizedSkeletalMesh::GetInstanceWorldBounds(Snapshot.Desc);
			}
		}
	}

	if (!WorldBox.IsValid)
	{
		WorldBox = FBox::BuildAABB(LocalToWorld.GetLocation(), FVector(1.0f));
	}

	return FBoxSphereBounds(WorldBox);
}

bool UOptimizedSkeletalMeshRenderComponent::ShouldRecreateProxyOnUpdateTransform() const
{
	return true;
}
