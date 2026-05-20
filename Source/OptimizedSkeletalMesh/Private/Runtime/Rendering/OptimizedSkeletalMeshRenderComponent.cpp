// Copyright Epic Games, Inc. All Rights Reserved.

#include "Runtime/Rendering/OptimizedSkeletalMeshRenderComponent.h"

#include "DynamicMeshBuilder.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "LocalVertexFactory.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialRenderProxy.h"
#include "MeshElementCollector.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "RawIndexBuffer.h"
#include "RenderDeferredCleanup.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Runtime/Manager/OptimizedSkeletalMeshWorldSubsystem.h"
#include "SceneManagement.h"

namespace OptimizedSkeletalMesh
{
	static constexpr float FallbackInstanceExtent = 50.0f;

	struct FRenderInstance
	{
		FBox WorldBounds;
		FMatrix44f LocalToWorld = FMatrix44f::Identity;
		int32 ForcedLODIndex = 0;
		bool bAutoLOD = true;
	};

	struct FCachedSectionMesh
	{
		TArray<FDynamicMeshVertex> Vertices;
		TArray<uint32> Indices;
		const FMaterialRenderProxy* MaterialRenderProxy = nullptr;
	};

	struct FDirectMeshResources : public FDeferredCleanupInterface
	{
		explicit FDirectMeshResources(ERHIFeatureLevel::Type InFeatureLevel)
			: VertexFactory(InFeatureLevel, "OptimizedSkeletalMeshDirectVF")
		{
		}

		void BeginDeferredRelease()
		{
			if (bInitialized)
			{
				BeginReleaseResource(&VertexFactory);
				bInitialized = false;
			}

			BeginCleanup(this);
		}

		void Init(const FSkeletalMeshLODRenderData& LODRenderData)
		{
			FPositionVertexBuffer* PositionVertexBuffer = const_cast<FPositionVertexBuffer*>(
				&LODRenderData.StaticVertexBuffers.PositionVertexBuffer);
			FStaticMeshVertexBuffer* StaticMeshVertexBuffer = const_cast<FStaticMeshVertexBuffer*>(
				&LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer);
			FColorVertexBuffer* ColorVertexBuffer = const_cast<FColorVertexBuffer*>(
				&LODRenderData.StaticVertexBuffers.ColorVertexBuffer);
			FLocalVertexFactory* VertexFactoryPtr = &VertexFactory;

			ENQUEUE_RENDER_COMMAND(InitOptimizedSkeletalMeshDirectVF)(
				[VertexFactoryPtr, PositionVertexBuffer, StaticMeshVertexBuffer, ColorVertexBuffer](FRHICommandList& RHICmdList)
				{
					FLocalVertexFactory::FDataType Data;
					PositionVertexBuffer->BindPositionVertexBuffer(VertexFactoryPtr, Data);
					StaticMeshVertexBuffer->BindTangentVertexBuffer(VertexFactoryPtr, Data);
					StaticMeshVertexBuffer->BindPackedTexCoordVertexBuffer(VertexFactoryPtr, Data);
					StaticMeshVertexBuffer->BindLightMapVertexBuffer(VertexFactoryPtr, Data, 0);
					ColorVertexBuffer->BindColorVertexBuffer(VertexFactoryPtr, Data);

					VertexFactoryPtr->SetData(RHICmdList, Data);
					VertexFactoryPtr->InitResource(RHICmdList);
				});

			bInitialized = true;
		}

		FLocalVertexFactory VertexFactory;
		bool bInitialized = false;
	};

	struct FLODResources
	{
		const FSkeletalMeshLODRenderData* LODRenderData = nullptr;
		TArray<FCachedSectionMesh> Sections;
		TUniquePtr<FDirectMeshResources> DirectResources;
		FColor DebugColor = FColor::Yellow;

		FLODResources() = default;
		FLODResources(const FLODResources&) = delete;
		FLODResources& operator=(const FLODResources&) = delete;
		FLODResources(FLODResources&&) = default;
		FLODResources& operator=(FLODResources&&) = default;

		~FLODResources()
		{
			if (DirectResources)
			{
				DirectResources->BeginDeferredRelease();
				OptimizedSkeletalMesh::FDirectMeshResources* ReleasedResources = DirectResources.Release();
				check(ReleasedResources);
			}
		}
	};

	struct FMeshRenderBatch
	{
		TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;
		TArray<FRenderInstance> Instances;
		TArray<TUniquePtr<FLODResources>> LODResources;
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

	static const FMaterialRenderProxy* GetWireframeMaterialRenderProxy()
	{
		if (GEngine && GEngine->WireframeMaterial)
		{
			return GEngine->WireframeMaterial->GetRenderProxy();
		}

		if (UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface))
		{
			return DefaultMaterial->GetRenderProxy();
		}

		return nullptr;
	}

	static FLinearColor GetLODColorationColor(const int32 LODIndex)
	{
		if (GEngine && GEngine->LODColorationColors.IsValidIndex(LODIndex))
		{
			return GEngine->LODColorationColors[LODIndex];
		}

		return FLinearColor(GetBatchDebugColor(LODIndex));
	}

	static const FMaterialRenderProxy* GetLODColorationMaterialRenderProxy(
		FMeshElementCollector& Collector,
		const int32 LODIndex)
	{
		const FMaterialRenderProxy* ParentRenderProxy = nullptr;
		if (GEngine && GEngine->ShadedLevelColorationUnlitMaterial)
		{
			ParentRenderProxy = GEngine->ShadedLevelColorationUnlitMaterial->GetRenderProxy();
		}
		else if (GEngine && GEngine->LevelColorationUnlitMaterial)
		{
			ParentRenderProxy = GEngine->LevelColorationUnlitMaterial->GetRenderProxy();
		}
		else if (UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface))
		{
			ParentRenderProxy = DefaultMaterial->GetRenderProxy();
		}

		return ParentRenderProxy
			? &Collector.AllocateOneFrameResource<FColoredMaterialRenderProxy>(
				ParentRenderProxy,
				GetLODColorationColor(LODIndex),
				NAME_Color)
			: nullptr;
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

	static const FSkeletalMeshLODRenderData* GetLODRenderData(const USkeletalMesh* SkeletalMesh, const int32 LODIndex)
	{
		if (!SkeletalMesh)
		{
			return nullptr;
		}

		const FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
		if (!RenderData || !RenderData->LODRenderData.IsValidIndex(LODIndex))
		{
			return nullptr;
		}

		return &RenderData->LODRenderData[LODIndex];
	}

	static int32 GetRenderLODCount(const USkeletalMesh* SkeletalMesh)
	{
		if (!SkeletalMesh)
		{
			return 0;
		}

		const FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
		return RenderData ? RenderData->LODRenderData.Num() : 0;
	}

	static float GetLODScreenSizeThreshold(const USkeletalMesh* SkeletalMesh, const int32 LODIndex)
	{
		if (SkeletalMesh)
		{
			if (const FSkeletalMeshLODInfo* LODInfo = SkeletalMesh->GetLODInfo(LODIndex))
			{
				return LODInfo->ScreenSize.Default;
			}
		}

		return FMath::Pow(0.5f, static_cast<float>(LODIndex));
	}

	static int32 ChooseLODForView(
		const FSceneView& View,
		const USkeletalMesh* SkeletalMesh,
		const FRenderInstance& Instance,
		const int32 NumLODs)
	{
		if (NumLODs <= 1)
		{
			return 0;
		}

		if (!Instance.bAutoLOD)
		{
			return FMath::Clamp(Instance.ForcedLODIndex, 0, NumLODs - 1);
		}

		const FBoxSphereBounds Bounds(Instance.WorldBounds);
		const float ScreenSize = ComputeBoundsScreenSize(
			FVector4(Bounds.Origin, 1.0f),
			Bounds.SphereRadius,
			View);

		int32 ChosenLOD = 0;
		for (int32 LODIndex = 1; LODIndex < NumLODs; ++LODIndex)
		{
			if (ScreenSize <= GetLODScreenSizeThreshold(SkeletalMesh, LODIndex))
			{
				ChosenLOD = LODIndex;
			}
		}

		return FMath::Clamp(ChosenLOD, 0, NumLODs - 1);
	}
}

class FOptimizedSkeletalMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	explicit FOptimizedSkeletalMeshSceneProxy(const UOptimizedSkeletalMeshRenderComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, bDrawDebugBounds(Component->ShouldDrawDebugBounds())
		, bDrawMeshSections(Component->ShouldDrawMeshSections())
		, MeshDrawMode(Component->GetMeshDrawMode())
		, MaxMeshDrawInstances(Component->GetMaxMeshDrawInstances())
	{
		if (const UWorld* World = Component->GetWorld())
		{
			if (const UOptimizedSkeletalMeshWorldSubsystem* Subsystem = World->GetSubsystem<UOptimizedSkeletalMeshWorldSubsystem>())
			{
				TArray<FOptimizedSkeletalMeshInstanceSnapshot> Snapshots;
				Subsystem->GetInstancesSnapshot(Snapshots);

				TMap<USkeletalMesh*, int32> BatchIndexByMesh;
				MeshBatches.Reserve(Snapshots.Num());

				for (const FOptimizedSkeletalMeshInstanceSnapshot& Snapshot : Snapshots)
				{
					if (!Snapshot.Desc.bVisible || !Snapshot.Desc.SkeletalMesh)
					{
						continue;
					}

					USkeletalMesh* SkeletalMesh = Snapshot.Desc.SkeletalMesh.Get();
					int32* ExistingBatchIndex = BatchIndexByMesh.Find(SkeletalMesh);

					if (!ExistingBatchIndex)
					{
						const int32 NewBatchIndex = MeshBatches.Num();
						OptimizedSkeletalMesh::FMeshRenderBatch& NewBatch = MeshBatches.AddDefaulted_GetRef();
						NewBatch.SkeletalMesh = SkeletalMesh;
						BatchIndexByMesh.Add(SkeletalMesh, NewBatchIndex);
						ExistingBatchIndex = BatchIndexByMesh.Find(SkeletalMesh);
					}

					OptimizedSkeletalMesh::FMeshRenderBatch& Batch = MeshBatches[*ExistingBatchIndex];
					OptimizedSkeletalMesh::FRenderInstance& RenderInstance = Batch.Instances.AddDefaulted_GetRef();
					RenderInstance.WorldBounds = OptimizedSkeletalMesh::GetInstanceWorldBounds(Snapshot.Desc);
					RenderInstance.LocalToWorld = FMatrix44f(Snapshot.Desc.WorldTransform.ToMatrixWithScale());
					RenderInstance.ForcedLODIndex = FMath::Max(0, Snapshot.Desc.LODIndex);
					RenderInstance.bAutoLOD = Snapshot.Desc.bAutoLOD;
				}

				for (OptimizedSkeletalMesh::FMeshRenderBatch& Batch : MeshBatches)
				{
					const int32 LODCount = OptimizedSkeletalMesh::GetRenderLODCount(Batch.SkeletalMesh);
					Batch.LODResources.Reserve(LODCount);
					for (int32 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
					{
						TUniquePtr<OptimizedSkeletalMesh::FLODResources> LODResources =
							MakeUnique<OptimizedSkeletalMesh::FLODResources>();
						LODResources->LODRenderData = OptimizedSkeletalMesh::GetLODRenderData(Batch.SkeletalMesh, LODIndex);
						LODResources->DebugColor = OptimizedSkeletalMesh::GetBatchDebugColor(LODIndex);

						if (bDrawMeshSections)
						{
							if (MeshDrawMode == EOptimizedSkeletalMeshDrawMode::DynamicMeshProof)
							{
								OptimizedSkeletalMesh::BuildCachedSectionMeshes(
									Batch.SkeletalMesh,
									LODIndex,
									LODResources->Sections);
							}
							else if (LODResources->LODRenderData)
							{
								LODResources->DirectResources =
									MakeUnique<OptimizedSkeletalMesh::FDirectMeshResources>(GetScene().GetFeatureLevel());
								LODResources->DirectResources->Init(*LODResources->LODRenderData);
							}
						}

						Batch.LODResources.Add(MoveTemp(LODResources));
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

			const bool bIsWireframeView = ViewFamily.EngineShowFlags.Wireframe;
			const bool bIsLODColorationView = ViewFamily.EngineShowFlags.LODColoration;
			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
			int32 DrawnMeshInstances = 0;

			for (const OptimizedSkeletalMesh::FMeshRenderBatch& Batch : MeshBatches)
			{
				for (const OptimizedSkeletalMesh::FRenderInstance& Instance : Batch.Instances)
				{
					const int32 ChosenLODIndex = OptimizedSkeletalMesh::ChooseLODForView(
						*Views[ViewIndex],
						Batch.SkeletalMesh,
						Instance,
						Batch.LODResources.Num());
					const OptimizedSkeletalMesh::FLODResources* LODResources =
						Batch.LODResources.IsValidIndex(ChosenLODIndex)
							? Batch.LODResources[ChosenLODIndex].Get()
							: nullptr;

					if (bDrawMeshSections && LODResources && (MaxMeshDrawInstances <= 0 || DrawnMeshInstances < MaxMeshDrawInstances))
					{
						if (MeshDrawMode == EOptimizedSkeletalMeshDrawMode::DynamicMeshProof)
						{
							for (const OptimizedSkeletalMesh::FCachedSectionMesh& Section : LODResources->Sections)
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
								FDynamicMeshBuilderSettings MeshSettings;
								MeshSettings.bWireframe = bIsWireframeView;
								MeshSettings.bDisableBackfaceCulling = false;
								MeshSettings.bReceivesDecals = true;
								MeshSettings.bUseSelectionOutline = true;
								MeshSettings.bCanApplyViewModeOverrides = true;
								const FMaterialRenderProxy* DynamicMaterialRenderProxy = bIsLODColorationView
									? OptimizedSkeletalMesh::GetLODColorationMaterialRenderProxy(Collector, ChosenLODIndex)
									: bIsWireframeView
										? OptimizedSkeletalMesh::GetWireframeMaterialRenderProxy()
										: Section.MaterialRenderProxy;
								if (!DynamicMaterialRenderProxy)
								{
									continue;
								}

								MeshBuilder.GetMesh(
									FMatrix(Instance.LocalToWorld),
									DynamicMaterialRenderProxy,
									SDPG_World,
									MeshSettings,
									nullptr,
									ViewIndex,
									Collector);
							}
						}
						else if (LODResources->LODRenderData && LODResources->DirectResources && LODResources->DirectResources->bInitialized)
						{
							for (const FSkelMeshRenderSection& RenderSection : LODResources->LODRenderData->RenderSections)
							{
								if (!RenderSection.IsValid())
								{
									continue;
								}

								const FMaterialRenderProxy* MaterialRenderProxy = bIsWireframeView
									? OptimizedSkeletalMesh::GetWireframeMaterialRenderProxy()
									: OptimizedSkeletalMesh::GetSectionMaterialRenderProxy(Batch.SkeletalMesh, RenderSection);
								if (!MaterialRenderProxy)
								{
									continue;
								}

								FMeshBatch& Mesh = Collector.AllocateMesh();
								Mesh.VertexFactory = &LODResources->DirectResources->VertexFactory;
								Mesh.MaterialRenderProxy = MaterialRenderProxy;
								Mesh.ReverseCulling = FMatrix(Instance.LocalToWorld).Determinant() < 0.0;
								Mesh.Type = PT_TriangleList;
								Mesh.DepthPriorityGroup = SDPG_World;
								Mesh.bCanApplyViewModeOverrides = true;
								Mesh.CastShadow = false;
								Mesh.bWireframe = bIsWireframeView;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
								Mesh.VisualizeLODIndex = ChosenLODIndex;
#endif

								FMeshBatchElement& BatchElement = Mesh.Elements[0];
								BatchElement.IndexBuffer = LODResources->LODRenderData->MultiSizeIndexContainer.GetIndexBuffer();
								BatchElement.FirstIndex = RenderSection.BaseIndex;
								BatchElement.NumPrimitives = RenderSection.NumTriangles;
								BatchElement.MinVertexIndex = RenderSection.BaseVertexIndex;
								BatchElement.MaxVertexIndex = RenderSection.BaseVertexIndex + RenderSection.NumVertices - 1;
								FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer =
									Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
								const FMatrix InstanceLocalToWorld(Instance.LocalToWorld);
								DynamicPrimitiveUniformBuffer.Set(
									Collector.GetRHICommandList(),
									InstanceLocalToWorld,
									InstanceLocalToWorld,
									Instance.WorldBounds,
									GetLocalBounds(),
									true,
									false,
									false);
								BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

								Collector.AddMesh(ViewIndex, Mesh);
							}
						}

						++DrawnMeshInstances;
					}

					if (bDrawDebugBounds)
					{
						const FColor DebugColor = LODResources
							? LODResources->DebugColor
							: OptimizedSkeletalMesh::GetBatchDebugColor(ChosenLODIndex);
						DrawWireBox(PDI, Instance.WorldBounds, DebugColor, SDPG_Foreground);
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
		SIZE_T AllocatedSize = FPrimitiveSceneProxy::GetAllocatedSize() + MeshBatches.GetAllocatedSize();
		for (const OptimizedSkeletalMesh::FMeshRenderBatch& Batch : MeshBatches)
		{
			AllocatedSize += Batch.Instances.GetAllocatedSize();
			AllocatedSize += Batch.LODResources.GetAllocatedSize();
			for (const TUniquePtr<OptimizedSkeletalMesh::FLODResources>& LODResources : Batch.LODResources)
			{
				if (!LODResources)
				{
					continue;
				}

				AllocatedSize += sizeof(OptimizedSkeletalMesh::FLODResources);
				AllocatedSize += LODResources->Sections.GetAllocatedSize();
				for (const OptimizedSkeletalMesh::FCachedSectionMesh& Section : LODResources->Sections)
				{
					AllocatedSize += Section.Vertices.GetAllocatedSize();
					AllocatedSize += Section.Indices.GetAllocatedSize();
				}
			}
		}

		return IntCastChecked<uint32>(AllocatedSize);
	}

private:
	TArray<OptimizedSkeletalMesh::FMeshRenderBatch> MeshBatches;
	bool bDrawDebugBounds = true;
	bool bDrawMeshSections = false;
	EOptimizedSkeletalMeshDrawMode MeshDrawMode = EOptimizedSkeletalMeshDrawMode::DynamicMeshProof;
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

void UOptimizedSkeletalMeshRenderComponent::SetMeshDrawMode(const EOptimizedSkeletalMeshDrawMode InMeshDrawMode)
{
	MeshDrawMode = InMeshDrawMode;
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

	if (GEngine && GEngine->WireframeMaterial)
	{
		OutMaterials.AddUnique(GEngine->WireframeMaterial);
	}

	if (GEngine && GEngine->LevelColorationUnlitMaterial)
	{
		OutMaterials.AddUnique(GEngine->LevelColorationUnlitMaterial);
	}

	if (GEngine && GEngine->ShadedLevelColorationUnlitMaterial)
	{
		OutMaterials.AddUnique(GEngine->ShadedLevelColorationUnlitMaterial);
	}
}
