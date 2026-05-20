// Copyright Epic Games, Inc. All Rights Reserved.

#include "Runtime/Rendering/OptimizedSkeletalMeshRenderComponent.h"

#include "DynamicMeshBuilder.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MeshElementCollector.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Runtime/Manager/OptimizedSkeletalMeshWorldSubsystem.h"
#include "SceneManagement.h"

namespace OptimizedSkeletalMesh
{
	static constexpr float FallbackInstanceExtent = 50.0f;

	struct FDebugInstance
	{
		FBox WorldBounds;
		FMatrix44f LocalToWorld = FMatrix44f::Identity;
	};

	struct FCachedSectionMesh
	{
		TArray<FDynamicMeshVertex> Vertices;
		TArray<uint32> Indices;
		const FMaterialRenderProxy* MaterialRenderProxy = nullptr;
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
		TArray<FCachedSectionMesh> Sections;
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

	static const FMaterialRenderProxy* GetSectionMaterialRenderProxy(const USkeletalMesh* SkeletalMesh, const FSkelMeshRenderSection& Section)
	{
		const TArray<FSkeletalMaterial>& Materials = SkeletalMesh->GetMaterials();
		UMaterialInterface* Material = Materials.IsValidIndex(Section.MaterialIndex)
			? Materials[Section.MaterialIndex].MaterialInterface
			: nullptr;

		if (!Material)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		return Material ? Material->GetRenderProxy() : nullptr;
	}

	static void BuildCachedSectionMeshes(const USkeletalMesh* SkeletalMesh, const int32 LODIndex, TArray<FCachedSectionMesh>& OutSections)
	{
		OutSections.Reset();

		if (!SkeletalMesh)
		{
			return;
		}

		FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
		if (!RenderData || !RenderData->LODRenderData.IsValidIndex(LODIndex))
		{
			return;
		}

		const FSkeletalMeshLODRenderData& LODRenderData = RenderData->LODRenderData[LODIndex];
		TArray<uint32> LODIndices;
		LODRenderData.MultiSizeIndexContainer.GetIndexBuffer(LODIndices);

		const uint32 NumVertices = LODRenderData.GetNumVertices();
		const uint32 NumTexCoords = LODRenderData.GetNumTexCoords();

		OutSections.Reserve(LODRenderData.RenderSections.Num());
		for (const FSkelMeshRenderSection& RenderSection : LODRenderData.RenderSections)
		{
			if (!RenderSection.IsValid())
			{
				continue;
			}

			FCachedSectionMesh& CachedSection = OutSections.AddDefaulted_GetRef();
			CachedSection.MaterialRenderProxy = GetSectionMaterialRenderProxy(SkeletalMesh, RenderSection);
			CachedSection.Vertices.Reserve(RenderSection.NumVertices);
			CachedSection.Indices.Reserve(RenderSection.NumTriangles * 3);

			TMap<uint32, int32> LocalVertexIndexByLODVertexIndex;
			LocalVertexIndexByLODVertexIndex.Reserve(RenderSection.NumVertices);

			for (uint32 TriangleIndex = 0; TriangleIndex < RenderSection.NumTriangles; ++TriangleIndex)
			{
				for (uint32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
				{
					const uint32 LODIndexBufferIndex = RenderSection.BaseIndex + TriangleIndex * 3 + CornerIndex;
					if (!LODIndices.IsValidIndex(LODIndexBufferIndex))
					{
						continue;
					}

					const uint32 LODVertexIndex = LODIndices[LODIndexBufferIndex];
					if (LODVertexIndex >= NumVertices)
					{
						continue;
					}

					int32* ExistingLocalVertexIndex = LocalVertexIndexByLODVertexIndex.Find(LODVertexIndex);
					if (!ExistingLocalVertexIndex)
					{
						FDynamicMeshVertex Vertex;
						Vertex.Position = LODRenderData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(LODVertexIndex);
						Vertex.TangentX = LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentX(LODVertexIndex);
						Vertex.TangentZ = LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(LODVertexIndex);
						Vertex.Color = FColor::White;

						const FVector2f UV = NumTexCoords > 0
							? LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV(LODVertexIndex, 0)
							: FVector2f::ZeroVector;

						for (int32 UVIndex = 0; UVIndex < MAX_STATIC_TEXCOORDS; ++UVIndex)
						{
							Vertex.TextureCoordinate[UVIndex] = UV;
						}

						const int32 NewLocalVertexIndex = CachedSection.Vertices.Add(Vertex);
						LocalVertexIndexByLODVertexIndex.Add(LODVertexIndex, NewLocalVertexIndex);
						ExistingLocalVertexIndex = LocalVertexIndexByLODVertexIndex.Find(LODVertexIndex);
					}

					CachedSection.Indices.Add(*ExistingLocalVertexIndex);
				}
			}
		}
	}
}

class FOptimizedSkeletalMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	explicit FOptimizedSkeletalMeshSceneProxy(const UOptimizedSkeletalMeshRenderComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, bDrawDebugBounds(Component->ShouldDrawDebugBounds())
		, bDrawMeshSections(Component->ShouldDrawMeshSections())
		, MaxMeshDrawInstances(Component->GetMaxMeshDrawInstances())
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
					DebugInstance.LocalToWorld = FMatrix44f(Snapshot.Desc.WorldTransform.ToMatrixWithScale());
				}

				if (bDrawMeshSections)
				{
					for (OptimizedSkeletalMesh::FRenderBatch& Batch : RenderBatches)
					{
						OptimizedSkeletalMesh::BuildCachedSectionMeshes(
							Batch.Key.SkeletalMesh,
							Batch.Key.LODIndex,
							Batch.Sections);
					}
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
				if (bDrawMeshSections)
				{
					const int32 MeshDrawInstanceCount = MaxMeshDrawInstances <= 0
						? Batch.Instances.Num()
						: FMath::Min(MaxMeshDrawInstances, Batch.Instances.Num());

					for (int32 InstanceIndex = 0; InstanceIndex < MeshDrawInstanceCount; ++InstanceIndex)
					{
						for (const OptimizedSkeletalMesh::FCachedSectionMesh& Section : Batch.Sections)
						{
							if (!Section.MaterialRenderProxy || Section.Vertices.IsEmpty() || Section.Indices.IsEmpty())
							{
								continue;
							}

							FDynamicMeshBuilder MeshBuilder(Views[ViewIndex]->GetFeatureLevel());
							MeshBuilder.ReserveVertices(Section.Vertices.Num());
							MeshBuilder.ReserveTriangles(Section.Indices.Num() / 3);
							MeshBuilder.AddVertices(Section.Vertices);
							MeshBuilder.AddTriangles(Section.Indices);
							MeshBuilder.GetMesh(
								FMatrix(Batch.Instances[InstanceIndex].LocalToWorld),
								Section.MaterialRenderProxy,
								SDPG_World,
								false,
								true,
								ViewIndex,
								Collector);
						}
					}
				}

				if (bDrawDebugBounds)
				{
					for (const OptimizedSkeletalMesh::FDebugInstance& DebugInstance : Batch.Instances)
					{
						DrawWireBox(PDI, DebugInstance.WorldBounds, Batch.DebugColor, SDPG_Foreground);
					}
				}
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bDynamicRelevance = true;
		Result.bOpaque = bDrawMeshSections;
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
			AllocatedSize += Batch.Sections.GetAllocatedSize();
			for (const OptimizedSkeletalMesh::FCachedSectionMesh& Section : Batch.Sections)
			{
				AllocatedSize += Section.Vertices.GetAllocatedSize();
				AllocatedSize += Section.Indices.GetAllocatedSize();
			}
		}

		return IntCastChecked<uint32>(AllocatedSize);
	}

private:
	TArray<OptimizedSkeletalMesh::FRenderBatch> RenderBatches;
	bool bDrawDebugBounds = true;
	bool bDrawMeshSections = false;
	int32 MaxMeshDrawInstances = 1;
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

void UOptimizedSkeletalMeshRenderComponent::SetDrawDebugBounds(const bool bInDrawDebugBounds)
{
	bDrawDebugBounds = bInDrawDebugBounds;
	RequestRenderRefresh();
}

void UOptimizedSkeletalMeshRenderComponent::SetDrawMeshSections(const bool bInDrawMeshSections)
{
	bDrawMeshSections = bInDrawMeshSections;
	RequestRenderRefresh();
}

void UOptimizedSkeletalMeshRenderComponent::SetMaxMeshDrawInstances(const int32 InMaxMeshDrawInstances)
{
	MaxMeshDrawInstances = FMath::Max(0, InMaxMeshDrawInstances);
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

void UOptimizedSkeletalMeshRenderComponent::GetUsedMaterials(
	TArray<UMaterialInterface*>& OutMaterials,
	bool bGetDebugMaterials) const
{
	Super::GetUsedMaterials(OutMaterials, bGetDebugMaterials);

	if (!Subsystem)
	{
		return;
	}

	TArray<FOptimizedSkeletalMeshInstanceSnapshot> Snapshots;
	Subsystem->GetInstancesSnapshot(Snapshots);

	for (const FOptimizedSkeletalMeshInstanceSnapshot& Snapshot : Snapshots)
	{
		if (!Snapshot.Desc.bVisible || !Snapshot.Desc.SkeletalMesh)
		{
			continue;
		}

		for (const FSkeletalMaterial& SkeletalMaterial : Snapshot.Desc.SkeletalMesh->GetMaterials())
		{
			if (SkeletalMaterial.MaterialInterface)
			{
				OutMaterials.AddUnique(SkeletalMaterial.MaterialInterface);
			}
		}
	}

	if (UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface))
	{
		OutMaterials.AddUnique(DefaultMaterial);
	}
}
