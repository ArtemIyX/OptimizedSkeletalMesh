// Copyright Epic Games, Inc. All Rights Reserved.

#include "Runtime/Rendering/OptimizedSkeletalMeshRenderComponent.h"

#include "Async/Async.h"
#include "DynamicMeshBuilder.h"
#include "ConvexVolume.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "InstanceUniformShaderParameters.h"
#include "LocalVertexFactory.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialRenderProxy.h"
#include "MeshElementCollector.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "RawIndexBuffer.h"
#include "RenderDeferredCleanup.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Runtime/Manager/OptimizedSkeletalMeshWorldSubsystem.h"
#include "SceneManagement.h"
#include "Stats/Stats.h"

DECLARE_STATS_GROUP_SORTBYNAME(TEXT("OptimizedSkeletalMesh Rendering"), STATGROUP_OptimizedSkeletalMeshRendering, STATCAT_Advanced);
DECLARE_STATS_GROUP_SORTBYNAME(TEXT("OptimizedSkeletalMesh Visible LOD"), STATGROUP_OptimizedSkeletalMeshVisibleLOD, STATCAT_Advanced);
DECLARE_STATS_GROUP_SORTBYNAME(TEXT("OptimizedSkeletalMesh Skinning"), STATGROUP_OptimizedSkeletalMeshSkinning, STATCAT_Advanced);
DECLARE_DWORD_COUNTER_STAT(TEXT("Registered Instances"), STAT_OptimizedSkeletalMeshRegisteredInstances, STATGROUP_OptimizedSkeletalMeshRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("Mesh Batches"), STAT_OptimizedSkeletalMeshMeshBatches, STATGROUP_OptimizedSkeletalMeshRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("Tested Instances"), STAT_OptimizedSkeletalMeshTestedInstances, STATGROUP_OptimizedSkeletalMeshRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("Visible Instances"), STAT_OptimizedSkeletalMeshVisibleInstances, STATGROUP_OptimizedSkeletalMeshRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("Culled Instances"), STAT_OptimizedSkeletalMeshCulledInstances, STATGROUP_OptimizedSkeletalMeshRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("Drawn Instances"), STAT_OptimizedSkeletalMeshDrawnInstances, STATGROUP_OptimizedSkeletalMeshRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("Submitted Draw Calls"), STAT_OptimizedSkeletalMeshSubmittedDrawCalls, STATGROUP_OptimizedSkeletalMeshRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("Submitted Sections"), STAT_OptimizedSkeletalMeshSubmittedSections, STATGROUP_OptimizedSkeletalMeshRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("Submitted Triangles"), STAT_OptimizedSkeletalMeshSubmittedTriangles, STATGROUP_OptimizedSkeletalMeshRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("LOD0"), STAT_OptimizedSkeletalMeshVisibleLOD0, STATGROUP_OptimizedSkeletalMeshVisibleLOD);
DECLARE_DWORD_COUNTER_STAT(TEXT("LOD1"), STAT_OptimizedSkeletalMeshVisibleLOD1, STATGROUP_OptimizedSkeletalMeshVisibleLOD);
DECLARE_DWORD_COUNTER_STAT(TEXT("LOD2"), STAT_OptimizedSkeletalMeshVisibleLOD2, STATGROUP_OptimizedSkeletalMeshVisibleLOD);
DECLARE_DWORD_COUNTER_STAT(TEXT("LOD3"), STAT_OptimizedSkeletalMeshVisibleLOD3, STATGROUP_OptimizedSkeletalMeshVisibleLOD);
DECLARE_DWORD_COUNTER_STAT(TEXT("LOD4"), STAT_OptimizedSkeletalMeshVisibleLOD4, STATGROUP_OptimizedSkeletalMeshVisibleLOD);
DECLARE_DWORD_COUNTER_STAT(TEXT("LOD5"), STAT_OptimizedSkeletalMeshVisibleLOD5, STATGROUP_OptimizedSkeletalMeshVisibleLOD);
DECLARE_DWORD_COUNTER_STAT(TEXT("LOD6"), STAT_OptimizedSkeletalMeshVisibleLOD6, STATGROUP_OptimizedSkeletalMeshVisibleLOD);
DECLARE_DWORD_COUNTER_STAT(TEXT("LOD7"), STAT_OptimizedSkeletalMeshVisibleLOD7, STATGROUP_OptimizedSkeletalMeshVisibleLOD);
DECLARE_DWORD_COUNTER_STAT(TEXT("Palette Instances"), STAT_OptimizedSkeletalMeshSkinningPaletteInstances, STATGROUP_OptimizedSkeletalMeshSkinning);
DECLARE_DWORD_COUNTER_STAT(TEXT("Palette Matrices"), STAT_OptimizedSkeletalMeshSkinningPaletteMatrices, STATGROUP_OptimizedSkeletalMeshSkinning);
DECLARE_MEMORY_STAT(TEXT("Palette Memory"), STAT_OptimizedSkeletalMeshSkinningPaletteBytes, STATGROUP_OptimizedSkeletalMeshSkinning);
DECLARE_DWORD_COUNTER_STAT(TEXT("GPU Palette Matrices"), STAT_OptimizedSkeletalMeshSkinningGPUPaletteMatrices, STATGROUP_OptimizedSkeletalMeshSkinning);
DECLARE_MEMORY_STAT(TEXT("GPU Palette Memory"), STAT_OptimizedSkeletalMeshSkinningGPUPaletteBytes, STATGROUP_OptimizedSkeletalMeshSkinning);
DECLARE_DWORD_COUNTER_STAT(TEXT("GPU Palette Uploads"), STAT_OptimizedSkeletalMeshSkinningGPUPaletteUploads, STATGROUP_OptimizedSkeletalMeshSkinning);

namespace OptimizedSkeletalMesh
{
	static constexpr float FallbackInstanceExtent = 50.0f;

	struct FRenderInstance
	{
		int32 InstanceId = INDEX_NONE;
		FBox WorldBounds;
		FMatrix44f LocalToWorld = FMatrix44f::Identity;
		int32 ForcedLODIndex = 0;
		bool bAutoLOD = true;
	};

	struct FBonePaletteRenderSnapshot
	{
		int32 InstanceId = INDEX_NONE;
		TArray<FMatrix44f> BonePalette;
	};

	struct FBonePaletteRange
	{
		uint32 Offset = 0;
		uint32 BoneCount = 0;
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

	struct FVisibleLODInstances
	{
		TArray<const FRenderInstance*> Instances;
	};

	static void AddVisibleLODStat(FOptimizedSkeletalMeshRenderStats& Stats, const int32 LODIndex, const int32 Count)
	{
		if (LODIndex < 0 || Count <= 0)
		{
			return;
		}

		if (Stats.VisibleInstancesByLOD.Num() <= LODIndex)
		{
			Stats.VisibleInstancesByLOD.SetNumZeroed(LODIndex + 1);
		}

		Stats.VisibleInstancesByLOD[LODIndex] += Count;
	}

	static int32 GetVisibleLODStat(const FOptimizedSkeletalMeshRenderStats& Stats, const int32 LODIndex)
	{
		return Stats.VisibleInstancesByLOD.IsValidIndex(LODIndex)
			? Stats.VisibleInstancesByLOD[LODIndex]
			: 0;
	}

	static const FSkeletalMeshLODRenderData* GetLODRenderData(const USkeletalMesh* SkeletalMesh, int32 LODIndex);

	static FBox GetInstanceWorldBounds(const FOptimizedSkeletalMeshInstanceDesc& Desc)
	{
		if (Desc.SkeletalMesh)
		{
			FBox LocalRenderBounds(ForceInit);
			if (const FSkeletalMeshLODRenderData* LODRenderData = GetLODRenderData(Desc.SkeletalMesh, 0))
			{
				const FPositionVertexBuffer& PositionVertexBuffer = LODRenderData->StaticVertexBuffers.PositionVertexBuffer;
				const uint32 NumVertices = PositionVertexBuffer.GetNumVertices();
				for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
				{
					LocalRenderBounds += static_cast<FVector>(PositionVertexBuffer.VertexPosition(VertexIndex));
				}
			}

			if (LocalRenderBounds.IsValid)
			{
				return LocalRenderBounds.TransformBy(Desc.WorldTransform);
			}

			return Desc.SkeletalMesh->GetBounds().GetBox().TransformBy(Desc.WorldTransform);
		}

		return FBox::BuildAABB(Desc.WorldTransform.GetLocation(), FVector(FallbackInstanceExtent));
	}

	static bool DoesClipSpaceBoxIntersectView(const FSceneView& View, const FBox& WorldBounds)
	{
		if (!WorldBounds.IsValid)
		{
			return false;
		}

		const FVector Min = WorldBounds.Min;
		const FVector Max = WorldBounds.Max;
		const FVector Corners[] =
		{
			FVector(Min.X, Min.Y, Min.Z),
			FVector(Min.X, Min.Y, Max.Z),
			FVector(Min.X, Max.Y, Min.Z),
			FVector(Min.X, Max.Y, Max.Z),
			FVector(Max.X, Min.Y, Min.Z),
			FVector(Max.X, Min.Y, Max.Z),
			FVector(Max.X, Max.Y, Min.Z),
			FVector(Max.X, Max.Y, Max.Z),
		};

		const FMatrix& ViewProjectionMatrix = View.ViewMatrices.GetViewProjectionMatrix();
		bool bAllLeft = true;
		bool bAllRight = true;
		bool bAllBelow = true;
		bool bAllAbove = true;
		bool bAllBehindNear = true;

		for (const FVector& Corner : Corners)
		{
			const FVector4 ClipPosition = ViewProjectionMatrix.TransformFVector4(FVector4(Corner, 1.0));
			const float ClipW = static_cast<float>(ClipPosition.W);

			if (ClipW <= UE_SMALL_NUMBER)
			{
				bAllBehindNear = false;
				continue;
			}

			bAllLeft &= ClipPosition.X < -ClipW;
			bAllRight &= ClipPosition.X > ClipW;
			bAllBelow &= ClipPosition.Y < -ClipW;
			bAllAbove &= ClipPosition.Y > ClipW;
			bAllBehindNear = false;
		}

		return !(bAllLeft || bAllRight || bAllBelow || bAllAbove || bAllBehindNear);
	}

	static FBox GetCullTestBounds(const FRenderInstance& Instance, const float InstanceCullBoundsScale)
	{
		if (!Instance.WorldBounds.IsValid)
		{
			return FBox(ForceInit);
		}

		return FBox::BuildAABB(
			Instance.WorldBounds.GetCenter(),
			Instance.WorldBounds.GetExtent() * FMath::Max(1.0f, InstanceCullBoundsScale));
	}

	static bool IsInstanceVisibleInView(
		const FSceneView& View,
		const FRenderInstance& Instance,
		const bool bEnableInstanceFrustumCulling,
		const float InstanceCullBoundsScale)
	{
		if (!bEnableInstanceFrustumCulling)
		{
			return true;
		}

		if (!Instance.WorldBounds.IsValid)
		{
			return false;
		}

		return DoesClipSpaceBoxIntersectView(View, GetCullTestBounds(Instance, InstanceCullBoundsScale));
	}

	static void BuildDynamicPrimitiveInstanceData(
		FMeshElementCollector& Collector,
		TConstArrayView<const FRenderInstance*> Instances,
		FMeshBatchDynamicPrimitiveData*& OutDynamicPrimitiveData,
		FBox& OutWorldBounds)
	{
		OutDynamicPrimitiveData = nullptr;
		OutWorldBounds = FBox(ForceInit);

		if (Instances.IsEmpty())
		{
			return;
		}

		TArray<FInstanceSceneData>& InstanceSceneData =
			Collector.AllocateOneFrameResource<TArray<FInstanceSceneData>>();
		InstanceSceneData.Reserve(Instances.Num());

		for (const FRenderInstance* Instance : Instances)
		{
			if (!Instance)
			{
				continue;
			}

			FInstanceSceneData& SceneData = InstanceSceneData.AddDefaulted_GetRef();
			SceneData.LocalToPrimitive = FRenderTransform(Instance->LocalToWorld);
			OutWorldBounds += Instance->WorldBounds;
		}

		if (InstanceSceneData.IsEmpty())
		{
			return;
		}

		FMeshBatchDynamicPrimitiveData& DynamicPrimitiveData =
			Collector.AllocateOneFrameResource<FMeshBatchDynamicPrimitiveData>();
		DynamicPrimitiveData.InstanceSceneData = MakeArrayView(InstanceSceneData);
		OutDynamicPrimitiveData = &DynamicPrimitiveData;
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
		, StatsComponent(const_cast<UOptimizedSkeletalMeshRenderComponent*>(Component))
		, bDrawDebugBounds(Component->ShouldDrawDebugBounds())
		, bDrawMeshSections(Component->ShouldDrawMeshSections())
		, MeshDrawMode(Component->GetMeshDrawMode())
		, MaxMeshDrawInstances(Component->GetMaxMeshDrawInstances())
		, bEnableInstanceFrustumCulling(Component->ShouldEnableInstanceFrustumCulling())
		, InstanceCullBoundsScale(Component->GetInstanceCullBoundsScale())
		, bDrawCullingDebug(Component->ShouldDrawCullingDebug())
		, bDrawCullTestBounds(Component->ShouldDrawCullTestBounds())
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
					RenderInstance.InstanceId = Snapshot.Handle.Id;
					RenderInstance.WorldBounds = OptimizedSkeletalMesh::GetInstanceWorldBounds(Snapshot.Desc);
					RenderInstance.LocalToWorld = FMatrix44f(Snapshot.Desc.WorldTransform.ToMatrixWithScale());
					RenderInstance.ForcedLODIndex = FMath::Max(0, Snapshot.Desc.LODIndex);
					RenderInstance.bAutoLOD = Snapshot.Desc.bAutoLOD;
					++RegisteredInstanceCount;
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

			FOptimizedSkeletalMeshRenderStats FrameStats;
			FrameStats.FrameNumber = static_cast<int32>(GFrameCounter);
			FrameStats.RegisteredInstances = RegisteredInstanceCount;
			FrameStats.MeshBatches = MeshBatches.Num();
			FrameStats.SkinningPaletteInstances = SkinningPaletteInstances;
			FrameStats.SkinningPaletteMatrices = SkinningPaletteMatrices;
			FrameStats.SkinningPaletteBytes = SkinningPaletteBytes;
			FrameStats.SkinningGPUPaletteMatrices = SkinningGPUPaletteMatrices;
			FrameStats.SkinningGPUPaletteBytes = SkinningGPUPaletteBytes;
			FrameStats.SkinningGPUPaletteUploads = SkinningGPUPaletteUploads;

			const bool bIsWireframeView = ViewFamily.EngineShowFlags.Wireframe;
			const bool bIsLODColorationView = ViewFamily.EngineShowFlags.LODColoration;
			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
			int32 DrawnMeshInstances = 0;

			for (const OptimizedSkeletalMesh::FMeshRenderBatch& Batch : MeshBatches)
			{
				if (bDrawMeshSections && MeshDrawMode == EOptimizedSkeletalMeshDrawMode::DirectMeshInstanced)
				{
					TArray<OptimizedSkeletalMesh::FVisibleLODInstances> VisibleInstancesByLOD;
					VisibleInstancesByLOD.SetNum(Batch.LODResources.Num());

					for (const OptimizedSkeletalMesh::FRenderInstance& Instance : Batch.Instances)
					{
						++FrameStats.TestedInstances;
						const bool bInstanceVisibleInView = OptimizedSkeletalMesh::IsInstanceVisibleInView(
							*Views[ViewIndex],
							Instance,
							bEnableInstanceFrustumCulling,
							InstanceCullBoundsScale);

						if (bDrawCullingDebug)
						{
							DrawWireBox(
								PDI,
								Instance.WorldBounds,
								bInstanceVisibleInView ? FColor::Green : FColor::Red,
								SDPG_Foreground);

							if (bDrawCullTestBounds)
							{
								const FBox CullTestBounds = OptimizedSkeletalMesh::GetCullTestBounds(Instance, InstanceCullBoundsScale);
								if (CullTestBounds.IsValid)
								{
									DrawWireBox(
										PDI,
										CullTestBounds,
										bInstanceVisibleInView ? FColor::Cyan : FColor::Orange,
										SDPG_Foreground);
								}
							}
						}

						if (!bInstanceVisibleInView || (MaxMeshDrawInstances > 0 && DrawnMeshInstances >= MaxMeshDrawInstances))
						{
							if (!bInstanceVisibleInView)
							{
								++FrameStats.CulledInstances;
							}
							continue;
						}

						++FrameStats.VisibleInstances;
						const int32 ChosenLODIndex = OptimizedSkeletalMesh::ChooseLODForView(
							*Views[ViewIndex],
							Batch.SkeletalMesh,
							Instance,
							Batch.LODResources.Num());

						if (!VisibleInstancesByLOD.IsValidIndex(ChosenLODIndex))
						{
							continue;
						}

						VisibleInstancesByLOD[ChosenLODIndex].Instances.Add(&Instance);
						++DrawnMeshInstances;
						++FrameStats.DrawnInstances;
						OptimizedSkeletalMesh::AddVisibleLODStat(FrameStats, ChosenLODIndex, 1);

						if (bDrawDebugBounds && !bDrawCullingDebug)
						{
							const OptimizedSkeletalMesh::FLODResources* LODResources =
								Batch.LODResources.IsValidIndex(ChosenLODIndex)
									? Batch.LODResources[ChosenLODIndex].Get()
									: nullptr;
							const FColor DebugColor = LODResources
								? LODResources->DebugColor
								: OptimizedSkeletalMesh::GetBatchDebugColor(ChosenLODIndex);
							DrawWireBox(PDI, Instance.WorldBounds, DebugColor, SDPG_Foreground);
						}
					}

					for (int32 LODIndex = 0; LODIndex < VisibleInstancesByLOD.Num(); ++LODIndex)
					{
						const OptimizedSkeletalMesh::FLODResources* LODResources =
							Batch.LODResources.IsValidIndex(LODIndex)
								? Batch.LODResources[LODIndex].Get()
								: nullptr;
						if (!LODResources || !LODResources->LODRenderData || !LODResources->DirectResources || !LODResources->DirectResources->bInitialized)
						{
							continue;
						}

						const TArray<const OptimizedSkeletalMesh::FRenderInstance*>& VisibleInstances =
							VisibleInstancesByLOD[LODIndex].Instances;
						if (VisibleInstances.IsEmpty())
						{
							continue;
						}

						FMeshBatchDynamicPrimitiveData* DynamicPrimitiveData = nullptr;
						FBox WorldBounds(ForceInit);
						OptimizedSkeletalMesh::BuildDynamicPrimitiveInstanceData(
							Collector,
							MakeArrayView(VisibleInstances),
							DynamicPrimitiveData,
							WorldBounds);
						if (!DynamicPrimitiveData || !WorldBounds.IsValid)
						{
							continue;
						}

						FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer =
							Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
						const FMatrix PrimitiveLocalToWorld = FMatrix::Identity;
						DynamicPrimitiveUniformBuffer.Set(
							Collector.GetRHICommandList(),
							PrimitiveLocalToWorld,
							PrimitiveLocalToWorld,
							WorldBounds,
							GetLocalBounds(),
							true,
							false,
							false);

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
							Mesh.ReverseCulling = false;
							Mesh.Type = PT_TriangleList;
							Mesh.DepthPriorityGroup = SDPG_World;
							Mesh.bCanApplyViewModeOverrides = true;
							Mesh.CastShadow = false;
							Mesh.bWireframe = bIsWireframeView;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
							Mesh.VisualizeLODIndex = LODIndex;
#endif

							FMeshBatchElement& BatchElement = Mesh.Elements[0];
							BatchElement.IndexBuffer = LODResources->LODRenderData->MultiSizeIndexContainer.GetIndexBuffer();
							BatchElement.FirstIndex = RenderSection.BaseIndex;
							BatchElement.NumPrimitives = RenderSection.NumTriangles;
							BatchElement.MinVertexIndex = RenderSection.BaseVertexIndex;
							BatchElement.MaxVertexIndex = RenderSection.BaseVertexIndex + RenderSection.NumVertices - 1;
							BatchElement.NumInstances = IntCastChecked<uint32>(VisibleInstances.Num());
							BatchElement.DynamicPrimitiveData = DynamicPrimitiveData;
							BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

							Collector.AddMesh(ViewIndex, Mesh);
							++FrameStats.SubmittedDrawCalls;
							++FrameStats.SubmittedSections;
							FrameStats.SubmittedTriangles += RenderSection.NumTriangles;
						}
					}

					continue;
				}

				for (const OptimizedSkeletalMesh::FRenderInstance& Instance : Batch.Instances)
				{
					++FrameStats.TestedInstances;
					const bool bInstanceVisibleInView = OptimizedSkeletalMesh::IsInstanceVisibleInView(
						*Views[ViewIndex],
						Instance,
						bEnableInstanceFrustumCulling,
						InstanceCullBoundsScale);

					if (bDrawCullingDebug)
					{
						DrawWireBox(
							PDI,
							Instance.WorldBounds,
							bInstanceVisibleInView ? FColor::Green : FColor::Red,
							SDPG_Foreground);

						if (bDrawCullTestBounds)
						{
							const FBox CullTestBounds = OptimizedSkeletalMesh::GetCullTestBounds(Instance, InstanceCullBoundsScale);
							if (CullTestBounds.IsValid)
							{
								DrawWireBox(
									PDI,
									CullTestBounds,
									bInstanceVisibleInView ? FColor::Cyan : FColor::Orange,
									SDPG_Foreground);
							}
						}
					}

					if (!bInstanceVisibleInView)
					{
						++FrameStats.CulledInstances;
						continue;
					}

					const int32 ChosenLODIndex = OptimizedSkeletalMesh::ChooseLODForView(
						*Views[ViewIndex],
						Batch.SkeletalMesh,
						Instance,
						Batch.LODResources.Num());
					++FrameStats.VisibleInstances;
					const OptimizedSkeletalMesh::FLODResources* LODResources =
						Batch.LODResources.IsValidIndex(ChosenLODIndex)
							? Batch.LODResources[ChosenLODIndex].Get()
							: nullptr;

					if (bDrawMeshSections && LODResources && (MaxMeshDrawInstances <= 0 || DrawnMeshInstances < MaxMeshDrawInstances))
					{
						++FrameStats.DrawnInstances;
						OptimizedSkeletalMesh::AddVisibleLODStat(FrameStats, ChosenLODIndex, 1);
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
								++FrameStats.SubmittedDrawCalls;
								++FrameStats.SubmittedSections;
								FrameStats.SubmittedTriangles += Section.Indices.Num() / 3;
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
								++FrameStats.SubmittedDrawCalls;
								++FrameStats.SubmittedSections;
								FrameStats.SubmittedTriangles += RenderSection.NumTriangles;
							}
						}

						++DrawnMeshInstances;
					}

					if (bDrawDebugBounds && !bDrawCullingDebug)
					{
						const FColor DebugColor = LODResources
							? LODResources->DebugColor
							: OptimizedSkeletalMesh::GetBatchDebugColor(ChosenLODIndex);
						DrawWireBox(PDI, Instance.WorldBounds, DebugColor, SDPG_Foreground);
					}
				}
			}

			PublishRenderStats(FrameStats);
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

	void PublishRenderStats(const FOptimizedSkeletalMeshRenderStats& Stats) const
	{
		if (!StatsComponent.IsValid())
		{
			return;
		}

		const TWeakObjectPtr<UOptimizedSkeletalMeshRenderComponent> WeakStatsComponent = StatsComponent;
		AsyncTask(
			ENamedThreads::GameThread,
			[WeakStatsComponent, Stats]()
			{
				if (UOptimizedSkeletalMeshRenderComponent* Component = WeakStatsComponent.Get())
				{
					Component->ApplyRenderStats_GameThread(Stats);
				}
			});
	}

	void UpdateBonePalettes_RenderThread(
		FRHICommandListImmediate& RHICmdList,
		TArray<OptimizedSkeletalMesh::FBonePaletteRenderSnapshot>&& InSnapshots)
	{
		check(IsInRenderingThread());

		BonePalettesByInstanceId.Reset();
		BonePalettesByInstanceId.Reserve(InSnapshots.Num());
		BonePaletteRangesByInstanceId.Reset();
		BonePaletteRangesByInstanceId.Reserve(InSnapshots.Num());
		PackedBonePalettes.Reset();
		SkinningPaletteMatrices = 0;
		SkinningPaletteBytes = 0;
		SkinningGPUPaletteMatrices = 0;
		SkinningGPUPaletteBytes = 0;

		for (OptimizedSkeletalMesh::FBonePaletteRenderSnapshot& Snapshot : InSnapshots)
		{
			const int32 BoneCount = Snapshot.BonePalette.Num();
			if (BoneCount <= 0)
			{
				continue;
			}

			const uint32 PaletteOffset = static_cast<uint32>(PackedBonePalettes.Num());
			OptimizedSkeletalMesh::FBonePaletteRange Range;
			Range.Offset = PaletteOffset;
			Range.BoneCount = static_cast<uint32>(BoneCount);
			BonePaletteRangesByInstanceId.Add(Snapshot.InstanceId, Range);

			PackedBonePalettes.Append(Snapshot.BonePalette);
			SkinningPaletteMatrices += BoneCount;
			SkinningPaletteBytes += BoneCount * sizeof(FMatrix44f);
			BonePalettesByInstanceId.Add(Snapshot.InstanceId, MoveTemp(Snapshot.BonePalette));
		}

		SkinningPaletteInstances = BonePalettesByInstanceId.Num();
		UploadBonePaletteBuffer_RenderThread(RHICmdList);
	}

	void UploadBonePaletteBuffer_RenderThread(FRHICommandListImmediate& RHICmdList)
	{
		check(IsInRenderingThread());

		if (PackedBonePalettes.IsEmpty())
		{
			BonePalettePooledBuffer.SafeRelease();
			SkinningGPUPaletteMatrices = 0;
			SkinningGPUPaletteBytes = 0;
			return;
		}

		FRDGBuilder GraphBuilder(
			RHICmdList,
			RDG_EVENT_NAME("OptimizedSkeletalMesh.UploadBonePalettes"));

		FRDGBufferRef BonePaletteBuffer = CreateStructuredBuffer(
			GraphBuilder,
			TEXT("OptimizedSkeletalMesh.BonePaletteBuffer"),
			static_cast<const TArray<FMatrix44f>&>(PackedBonePalettes));

		GraphBuilder.QueueBufferExtraction(
			BonePaletteBuffer,
			&BonePalettePooledBuffer,
			ERHIAccess::SRVMask);
		GraphBuilder.Execute();

		++SkinningGPUPaletteUploads;
		SkinningGPUPaletteMatrices = PackedBonePalettes.Num();
		SkinningGPUPaletteBytes = PackedBonePalettes.Num() * sizeof(FMatrix44f);
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

		AllocatedSize += BonePalettesByInstanceId.GetAllocatedSize();
		for (const TPair<int32, TArray<FMatrix44f>>& Pair : BonePalettesByInstanceId)
		{
			AllocatedSize += Pair.Value.GetAllocatedSize();
		}
		AllocatedSize += BonePaletteRangesByInstanceId.GetAllocatedSize();
		AllocatedSize += PackedBonePalettes.GetAllocatedSize();

		return IntCastChecked<uint32>(AllocatedSize);
	}

private:
	TWeakObjectPtr<UOptimizedSkeletalMeshRenderComponent> StatsComponent;
	TArray<OptimizedSkeletalMesh::FMeshRenderBatch> MeshBatches;
	TMap<int32, TArray<FMatrix44f>> BonePalettesByInstanceId;
	TMap<int32, OptimizedSkeletalMesh::FBonePaletteRange> BonePaletteRangesByInstanceId;
	TArray<FMatrix44f> PackedBonePalettes;
	TRefCountPtr<FRDGPooledBuffer> BonePalettePooledBuffer;
	int32 RegisteredInstanceCount = 0;
	int32 SkinningPaletteInstances = 0;
	int32 SkinningPaletteMatrices = 0;
	int32 SkinningPaletteBytes = 0;
	int32 SkinningGPUPaletteMatrices = 0;
	int32 SkinningGPUPaletteBytes = 0;
	int32 SkinningGPUPaletteUploads = 0;
	bool bDrawDebugBounds = true;
	bool bDrawMeshSections = false;
	EOptimizedSkeletalMeshDrawMode MeshDrawMode = EOptimizedSkeletalMeshDrawMode::DynamicMeshProof;
	int32 MaxMeshDrawInstances = 1;
	bool bEnableInstanceFrustumCulling = true;
	float InstanceCullBoundsScale = 1.5f;
	bool bDrawCullingDebug = false;
	bool bDrawCullTestBounds = true;
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

void UOptimizedSkeletalMeshRenderComponent::SetInstanceFrustumCulling(const bool bInEnableInstanceFrustumCulling)
{
	bEnableInstanceFrustumCulling = bInEnableInstanceFrustumCulling;
	RequestRenderRefresh();
}

void UOptimizedSkeletalMeshRenderComponent::SetInstanceCullBoundsScale(const float InInstanceCullBoundsScale)
{
	InstanceCullBoundsScale = FMath::Max(1.0f, InInstanceCullBoundsScale);
	RequestRenderRefresh();
}

void UOptimizedSkeletalMeshRenderComponent::SetConservativeProxyBounds(const bool bInUseConservativeProxyBounds)
{
	bUseConservativeProxyBounds = bInUseConservativeProxyBounds;
	RequestRenderRefresh();
}

void UOptimizedSkeletalMeshRenderComponent::SetConservativeProxyBoundsExtent(const float InConservativeProxyBoundsExtent)
{
	ConservativeProxyBoundsExtent = FMath::Max(1000.0f, InConservativeProxyBoundsExtent);
	RequestRenderRefresh();
}

void UOptimizedSkeletalMeshRenderComponent::SetDrawCullingDebug(const bool bInDrawCullingDebug)
{
	bDrawCullingDebug = bInDrawCullingDebug;
	RequestRenderRefresh();
}

void UOptimizedSkeletalMeshRenderComponent::SetDrawCullTestBounds(const bool bInDrawCullTestBounds)
{
	bDrawCullTestBounds = bInDrawCullTestBounds;
	RequestRenderRefresh();
}

void UOptimizedSkeletalMeshRenderComponent::PushBonePalettesToRenderThread()
{
	check(IsInGameThread());

	if (!Subsystem || !SceneProxy)
	{
		return;
	}

	TArray<FOptimizedSkeletalMeshBonePaletteSnapshot> PaletteSnapshots;
	Subsystem->GetBonePaletteSnapshots(PaletteSnapshots);

	TArray<OptimizedSkeletalMesh::FBonePaletteRenderSnapshot> RenderSnapshots;
	RenderSnapshots.Reserve(PaletteSnapshots.Num());

	for (FOptimizedSkeletalMeshBonePaletteSnapshot& Snapshot : PaletteSnapshots)
	{
		if (!Snapshot.Handle.IsValid() || Snapshot.BonePalette.IsEmpty())
		{
			continue;
		}

		OptimizedSkeletalMesh::FBonePaletteRenderSnapshot& RenderSnapshot = RenderSnapshots.AddDefaulted_GetRef();
		RenderSnapshot.InstanceId = Snapshot.Handle.Id;
		RenderSnapshot.BonePalette = MoveTemp(Snapshot.BonePalette);
	}

	FOptimizedSkeletalMeshSceneProxy* OptimizedSceneProxy =
		static_cast<FOptimizedSkeletalMeshSceneProxy*>(SceneProxy);

	ENQUEUE_RENDER_COMMAND(UpdateOptimizedSkeletalMeshBonePalettes)(
		[OptimizedSceneProxy, RenderSnapshots = MoveTemp(RenderSnapshots)](FRHICommandListImmediate& RHICmdList) mutable
		{
			OptimizedSceneProxy->UpdateBonePalettes_RenderThread(RHICmdList, MoveTemp(RenderSnapshots));
		});
}

void UOptimizedSkeletalMeshRenderComponent::ApplyRenderStats_GameThread(
	const FOptimizedSkeletalMeshRenderStats& InStats)
{
	check(IsInGameThread());
	LastRenderStats = InStats;

	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshRegisteredInstances, InStats.RegisteredInstances);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshMeshBatches, InStats.MeshBatches);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshTestedInstances, InStats.TestedInstances);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshVisibleInstances, InStats.VisibleInstances);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshCulledInstances, InStats.CulledInstances);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshDrawnInstances, InStats.DrawnInstances);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSubmittedDrawCalls, InStats.SubmittedDrawCalls);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSubmittedSections, InStats.SubmittedSections);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSubmittedTriangles, InStats.SubmittedTriangles);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSkinningPaletteInstances, InStats.SkinningPaletteInstances);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSkinningPaletteMatrices, InStats.SkinningPaletteMatrices);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSkinningPaletteBytes, InStats.SkinningPaletteBytes);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSkinningGPUPaletteMatrices, InStats.SkinningGPUPaletteMatrices);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSkinningGPUPaletteBytes, InStats.SkinningGPUPaletteBytes);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSkinningGPUPaletteUploads, InStats.SkinningGPUPaletteUploads);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshVisibleLOD0, OptimizedSkeletalMesh::GetVisibleLODStat(InStats, 0));
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshVisibleLOD1, OptimizedSkeletalMesh::GetVisibleLODStat(InStats, 1));
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshVisibleLOD2, OptimizedSkeletalMesh::GetVisibleLODStat(InStats, 2));
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshVisibleLOD3, OptimizedSkeletalMesh::GetVisibleLODStat(InStats, 3));
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshVisibleLOD4, OptimizedSkeletalMesh::GetVisibleLODStat(InStats, 4));
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshVisibleLOD5, OptimizedSkeletalMesh::GetVisibleLODStat(InStats, 5));
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshVisibleLOD6, OptimizedSkeletalMesh::GetVisibleLODStat(InStats, 6));
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshVisibleLOD7, OptimizedSkeletalMesh::GetVisibleLODStat(InStats, 7));
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

	if (bUseConservativeProxyBounds)
	{
		return FBoxSphereBounds(FBox::BuildAABB(WorldBox.GetCenter(), FVector(ConservativeProxyBoundsExtent)));
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
