// Copyright Epic Games, Inc. All Rights Reserved.

#include "Runtime/Rendering/OptimizedSkeletalMeshRenderComponent.h"

#include "Async/Async.h"
#include "DynamicMeshBuilder.h"
#include "DynamicBufferAllocator.h"
#include "ConvexVolume.h"
#include "Engine/Engine.h"
#include "Engine/skeletalMesh.h"
#include "Engine/world.h"
#include "GlobalRenderResources.h"
#include "InstanceUniformShaderParameters.h"
#include "LocalVertexFactory.h"
#include "materials/material.h"
#include "materials/MaterialInterface.h"
#include "materials/MaterialRenderProxy.h"
#include "MeshDrawShaderBindings.h"
#include "MeshElementCollector.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "RawIndexBuffer.h"
#include "RenderDeferredCleanup.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Runtime/Manager/OptimizedSkeletalMeshWorldSubsystem.h"
#include "SceneManagement.h"
#include "Stats/Stats.h"

DECLARE_STATS_GROUP_SORTBYNAME(TEXT("OSM Rendering"), STATGROUP_OSMRendering, STATCAT_Advanced);
DECLARE_STATS_GROUP_SORTBYNAME(TEXT("OSM Visible LOD"), STATGROUP_OSMVisibleLOD, STATCAT_Advanced);
DECLARE_STATS_GROUP_SORTBYNAME(TEXT("OSM Skinning"), STATGROUP_OSMSkinning, STATCAT_Advanced);
DECLARE_GPU_STAT_NAMED(OSMRender, TEXT("OptimizedSkeletalMesh"));
DECLARE_DWORD_COUNTER_STAT(TEXT("Registered InInstances"), STAT_OptimizedSkeletalMeshRegisteredInstances, STATGROUP_OSMRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("mesh Batches"), STAT_OptimizedSkeletalMeshMeshBatches, STATGROUP_OSMRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("Tested InInstances"), STAT_OptimizedSkeletalMeshTestedInstances, STATGROUP_OSMRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("Visible InInstances"), STAT_OptimizedSkeletalMeshVisibleInstances, STATGROUP_OSMRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("Culled InInstances"), STAT_OptimizedSkeletalMeshCulledInstances, STATGROUP_OSMRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("Drawn InInstances"), STAT_OptimizedSkeletalMeshDrawnInstances, STATGROUP_OSMRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("Submitted Draw Calls"), STAT_OptimizedSkeletalMeshSubmittedDrawCalls, STATGROUP_OSMRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("Submitted Sections"), STAT_OptimizedSkeletalMeshSubmittedSections, STATGROUP_OSMRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("Submitted Triangles"), STAT_OptimizedSkeletalMeshSubmittedTriangles, STATGROUP_OSMRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("LOD0"), STAT_OptimizedSkeletalMeshVisibleLOD0, STATGROUP_OSMVisibleLOD);
DECLARE_DWORD_COUNTER_STAT(TEXT("LOD1"), STAT_OptimizedSkeletalMeshVisibleLOD1, STATGROUP_OSMVisibleLOD);
DECLARE_DWORD_COUNTER_STAT(TEXT("LOD2"), STAT_OptimizedSkeletalMeshVisibleLOD2, STATGROUP_OSMVisibleLOD);
DECLARE_DWORD_COUNTER_STAT(TEXT("LOD3"), STAT_OptimizedSkeletalMeshVisibleLOD3, STATGROUP_OSMVisibleLOD);
DECLARE_DWORD_COUNTER_STAT(TEXT("LOD4"), STAT_OptimizedSkeletalMeshVisibleLOD4, STATGROUP_OSMVisibleLOD);
DECLARE_DWORD_COUNTER_STAT(TEXT("LOD5"), STAT_OptimizedSkeletalMeshVisibleLOD5, STATGROUP_OSMVisibleLOD);
DECLARE_DWORD_COUNTER_STAT(TEXT("LOD6"), STAT_OptimizedSkeletalMeshVisibleLOD6, STATGROUP_OSMVisibleLOD);
DECLARE_DWORD_COUNTER_STAT(TEXT("LOD7"), STAT_OptimizedSkeletalMeshVisibleLOD7, STATGROUP_OSMVisibleLOD);
DECLARE_DWORD_COUNTER_STAT(TEXT("Palette InInstances"), STAT_OptimizedSkeletalMeshSkinningPaletteInstances, STATGROUP_OSMSkinning);
DECLARE_DWORD_COUNTER_STAT(TEXT("Palette Matrices"), STAT_OptimizedSkeletalMeshSkinningPaletteMatrices, STATGROUP_OSMSkinning);
DECLARE_MEMORY_STAT(TEXT("Palette Memory"), STAT_OptimizedSkeletalMeshSkinningPaletteBytes, STATGROUP_OSMSkinning);
DECLARE_DWORD_COUNTER_STAT(TEXT("GPU Palette Matrices"), STAT_OptimizedSkeletalMeshSkinningGPUPaletteMatrices, STATGROUP_OSMSkinning);
DECLARE_MEMORY_STAT(TEXT("GPU Palette Memory"), STAT_OptimizedSkeletalMeshSkinningGPUPaletteBytes, STATGROUP_OSMSkinning);
DECLARE_DWORD_COUNTER_STAT(TEXT("GPU Palette Uploads"), STAT_OptimizedSkeletalMeshSkinningGPUPaletteUploads, STATGROUP_OSMSkinning);
DECLARE_DWORD_COUNTER_STAT(TEXT("Skin Weight LODs"), STAT_OptimizedSkeletalMeshSkinningSkinWeightLODs, STATGROUP_OSMSkinning);
DECLARE_DWORD_COUNTER_STAT(TEXT("Skin Weight Vertices"), STAT_OptimizedSkeletalMeshSkinningSkinWeightVertices, STATGROUP_OSMSkinning);
DECLARE_DWORD_COUNTER_STAT(TEXT("max Bone Influences"), STAT_OptimizedSkeletalMeshSkinningMaxBoneInfluences, STATGROUP_OSMSkinning);
DECLARE_DWORD_COUNTER_STAT(TEXT("16-bit Bone Index LODs"), STAT_OptimizedSkeletalMeshSkinning16BitIndexLODs, STATGROUP_OSMSkinning);
DECLARE_DWORD_COUNTER_STAT(TEXT("16-bit Bone Weight LODs"), STAT_OptimizedSkeletalMeshSkinning16BitWeightLODs, STATGROUP_OSMSkinning);
DECLARE_DWORD_COUNTER_STAT(TEXT("Missing Skin Weight LODs"), STAT_OptimizedSkeletalMeshSkinningMissingSkinWeightLODs, STATGROUP_OSMSkinning);
DECLARE_DWORD_COUNTER_STAT(TEXT("GPU Skin Ready LODs"), STAT_OptimizedSkeletalMeshSkinningGPUSkinReadyLODs, STATGROUP_OSMSkinning);
DECLARE_DWORD_COUNTER_STAT(TEXT("GPU Skin Fallback Draws"), STAT_OptimizedSkeletalMeshSkinningGPUSkinFallbackDraws, STATGROUP_OSMSkinning);
DECLARE_DWORD_COUNTER_STAT(TEXT("GPU Skin Draws"), STAT_OptimizedSkeletalMeshSkinningGPUSkinDraws, STATGROUP_OSMSkinning);
DECLARE_DWORD_COUNTER_STAT(TEXT("GPU Instance Offset Entries"), STAT_OptimizedSkeletalMeshSkinningGPUInstanceOffsetEntries, STATGROUP_OSMSkinning);
DECLARE_DWORD_COUNTER_STAT(TEXT("GPU Section Bone Map Entries"), STAT_OptimizedSkeletalMeshSkinningGPUSectionBoneMapEntries, STATGROUP_OSMSkinning);

struct FOptimizedSkeletalMeshVertexFactoryUserData : public FOneFrameResource
{
	FRHIShaderResourceView* SkinWeightDataSRV = nullptr;
	FRHIShaderResourceView* SkinWeightLookupSRV = nullptr;
	FRHIShaderResourceView* BonePaletteSRV = nullptr;
	FRHIShaderResourceView* InstancePaletteOffsetSRV = nullptr;
	FRHIShaderResourceView* SectionBoneMapSRV = nullptr;
	uint32 BonePaletteMatrixCount = 0;
	uint32 MaxBoneInfluences = 0;
	uint32 BoneIndexByteSize = 0;
	uint32 BoneWeightByteSize = 0;
	uint32 SkinWeightStride = 0;
	uint32 SkinWeightBoneWeightsOffset = 0;
	uint32 InstancePaletteOffsetCount = 0;
	uint32 SectionBoneMapCount = 0;
	uint32 bVariableBonesPerVertex = 0;
};

class FOptimizedSkeletalMeshVertexFactory final : public FLocalVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FOptimizedSkeletalMeshVertexFactory);

public:
	explicit FOptimizedSkeletalMeshVertexFactory(const ERHIFeatureLevel::Type InFeatureLevel)
		: FLocalVertexFactory(InFeatureLevel, "OptimizedSkeletalMeshVF")
	{
	}

	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& InParameters)
	{
		return FLocalVertexFactory::ShouldCompilePermutation(InParameters);
	}

	static void ModifyCompilationEnvironment(
		const FVertexFactoryShaderPermutationParameters& InParameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FLocalVertexFactory::ModifyCompilationEnvironment(InParameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("OPTIMIZED_SKELETAL_MESH_VERTEX_FACTORY"), 1);
	}

	static void ValidateCompiledResult(
		const FVertexFactoryType* InType,
		const EShaderPlatform InPlatform,
		const FShaderParameterMap& InParameterMap,
		TArray<FString>& OutErrors)
	{
		FLocalVertexFactory::ValidateCompiledResult(InType, InPlatform, InParameterMap, OutErrors);
	}
};

class FOptimizedSkeletalMeshVertexFactoryShaderParameters final : public FLocalVertexFactoryShaderParametersBase
{
	DECLARE_TYPE_LAYOUT(FOptimizedSkeletalMeshVertexFactoryShaderParameters, NonVirtual);

public:
	void Bind(const FShaderParameterMap& InParameterMap)
	{
		FLocalVertexFactoryShaderParametersBase::Bind(InParameterMap);
		SkinWeightDataParameter.Bind(InParameterMap, TEXT("OptimizedSkinWeightData"));
		SkinWeightLookupParameter.Bind(InParameterMap, TEXT("OptimizedSkinWeightLookup"));
		BonePaletteParameter.Bind(InParameterMap, TEXT("OptimizedBonePalette"));
		InstancePaletteOffsetParameter.Bind(InParameterMap, TEXT("OptimizedInstancePaletteOffsets"));
		SectionBoneMapParameter.Bind(InParameterMap, TEXT("OptimizedSectionBoneMap"));
		BonePaletteMatrixCountParameter.Bind(InParameterMap, TEXT("OptimizedBonePaletteMatrixCount"));
		MaxBoneInfluencesParameter.Bind(InParameterMap, TEXT("OptimizedMaxBoneInfluences"));
		BoneIndexByteSizeParameter.Bind(InParameterMap, TEXT("OptimizedBoneIndexByteSize"));
		BoneWeightByteSizeParameter.Bind(InParameterMap, TEXT("OptimizedBoneWeightByteSize"));
		SkinWeightStrideParameter.Bind(InParameterMap, TEXT("OptimizedSkinWeightStride"));
		SkinWeightBoneWeightsOffsetParameter.Bind(InParameterMap, TEXT("OptimizedSkinWeightBoneWeightsOffset"));
		InstancePaletteOffsetCountParameter.Bind(InParameterMap, TEXT("OptimizedInstancePaletteOffsetCount"));
		SectionBoneMapCountParameter.Bind(InParameterMap, TEXT("OptimizedSectionBoneMapCount"));
		VariableBonesPerVertexParameter.Bind(InParameterMap, TEXT("OptimizedVariableBonesPerVertex"));
	}

	void GetElementShaderBindings(
		const FSceneInterface* InScene,
		const FSceneView* InView,
		const FMeshMaterialShader* InShader,
		const EVertexInputStreamType InInputStreamType,
		ERHIFeatureLevel::Type InFeatureLevel,
		const FVertexFactory* InVertexFactory,
		const FMeshBatchElement& batchElement,
		FMeshDrawSingleShaderBindings& InShaderBindings,
		FVertexInputStreamArray& InVertexStreams) const
	{
		FRHIUniformBuffer* vertexFactoryUniformBuffer =
			static_cast<FRHIUniformBuffer*>(batchElement.VertexFactoryUserData);

		GetElementShaderBindingsBase(
			InScene,
			InView,
			InShader,
			InInputStreamType,
			InFeatureLevel,
			InVertexFactory,
			batchElement,
			vertexFactoryUniformBuffer,
			InShaderBindings,
			InVertexStreams);

		const FOptimizedSkeletalMeshVertexFactoryUserData* userData =
			reinterpret_cast<const FOptimizedSkeletalMeshVertexFactoryUserData*>(batchElement.UserData);
		if (userData)
		{
			InShaderBindings.Add(
				SkinWeightDataParameter,
				userData->SkinWeightDataSRV ? userData->SkinWeightDataSRV : GNullColorVertexBuffer.VertexBufferSRV.GetReference());
			InShaderBindings.Add(
				SkinWeightLookupParameter,
				userData->SkinWeightLookupSRV ? userData->SkinWeightLookupSRV : GNullColorVertexBuffer.VertexBufferSRV.GetReference());
			InShaderBindings.Add(
				BonePaletteParameter,
				userData->BonePaletteSRV ? userData->BonePaletteSRV : GNullColorVertexBuffer.VertexBufferSRV.GetReference());
			InShaderBindings.Add(
				InstancePaletteOffsetParameter,
				userData->InstancePaletteOffsetSRV ? userData->InstancePaletteOffsetSRV : GNullColorVertexBuffer.VertexBufferSRV.GetReference());
			InShaderBindings.Add(
				SectionBoneMapParameter,
				userData->SectionBoneMapSRV ? userData->SectionBoneMapSRV : GNullColorVertexBuffer.VertexBufferSRV.GetReference());
			InShaderBindings.Add(BonePaletteMatrixCountParameter, userData->BonePaletteMatrixCount);
			InShaderBindings.Add(MaxBoneInfluencesParameter, userData->MaxBoneInfluences);
			InShaderBindings.Add(BoneIndexByteSizeParameter, userData->BoneIndexByteSize);
			InShaderBindings.Add(BoneWeightByteSizeParameter, userData->BoneWeightByteSize);
			InShaderBindings.Add(SkinWeightStrideParameter, userData->SkinWeightStride);
			InShaderBindings.Add(SkinWeightBoneWeightsOffsetParameter, userData->SkinWeightBoneWeightsOffset);
			InShaderBindings.Add(InstancePaletteOffsetCountParameter, userData->InstancePaletteOffsetCount);
			InShaderBindings.Add(SectionBoneMapCountParameter, userData->SectionBoneMapCount);
			InShaderBindings.Add(VariableBonesPerVertexParameter, userData->bVariableBonesPerVertex);
		}
	}

private:
	LAYOUT_FIELD(FShaderResourceParameter, SkinWeightDataParameter);
	LAYOUT_FIELD(FShaderResourceParameter, SkinWeightLookupParameter);
	LAYOUT_FIELD(FShaderResourceParameter, BonePaletteParameter);
	LAYOUT_FIELD(FShaderResourceParameter, InstancePaletteOffsetParameter);
	LAYOUT_FIELD(FShaderResourceParameter, SectionBoneMapParameter);
	LAYOUT_FIELD(FShaderParameter, BonePaletteMatrixCountParameter);
	LAYOUT_FIELD(FShaderParameter, MaxBoneInfluencesParameter);
	LAYOUT_FIELD(FShaderParameter, BoneIndexByteSizeParameter);
	LAYOUT_FIELD(FShaderParameter, BoneWeightByteSizeParameter);
	LAYOUT_FIELD(FShaderParameter, SkinWeightStrideParameter);
	LAYOUT_FIELD(FShaderParameter, SkinWeightBoneWeightsOffsetParameter);
	LAYOUT_FIELD(FShaderParameter, InstancePaletteOffsetCountParameter);
	LAYOUT_FIELD(FShaderParameter, SectionBoneMapCountParameter);
	LAYOUT_FIELD(FShaderParameter, VariableBonesPerVertexParameter);
};

IMPLEMENT_TYPE_LAYOUT(FOptimizedSkeletalMeshVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FOptimizedSkeletalMeshVertexFactory, SF_Vertex, FOptimizedSkeletalMeshVertexFactoryShaderParameters);

IMPLEMENT_VERTEX_FACTORY_TYPE(
	FOptimizedSkeletalMeshVertexFactory,
	"/Plugin/OptimizedSkeletalMesh/Private/OptimizedSkeletalMeshVertexFactory.ush",
	EVertexFactoryFlags::UsedWithMaterials
		| EVertexFactoryFlags::SupportsDynamicLighting
		| EVertexFactoryFlags::SupportsPrecisePrevWorldPos
		| EVertexFactoryFlags::SupportsPositionOnly
		| EVertexFactoryFlags::SupportsPrimitiveIdStream
		| EVertexFactoryFlags::SupportsManualVertexFetch
		| EVertexFactoryFlags::SupportsPSOPrecaching
		| EVertexFactoryFlags::SupportsTriangleSorting);

namespace OptimizedSkeletalMesh
{
	static constexpr float FallbackInstanceExtent = 50.0f;

	struct FRenderInstance
	{
		int32 InstanceId = INDEX_NONE;
		FBox worldBounds;
		FMatrix44f InLocalToWorld = FMatrix44f::Identity;
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
		uint32 boneCount = 0;
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
			, SkinnedVertexFactory(InFeatureLevel)
		{
		}

		void BeginDeferredRelease()
		{
			if (bInitialized)
			{
				BeginReleaseResource(&VertexFactory);
				BeginReleaseResource(&SkinnedVertexFactory);
				bInitialized = false;
			}

			BeginCleanup(this);
		}

		void Init(const FSkeletalMeshLODRenderData& lodRenderData)
		{
			SkinWeightVertexBuffer = &lodRenderData.SkinWeightVertexBuffer;
			SkinWeightVertexCount = IntCastChecked<int32>(SkinWeightVertexBuffer->GetNumVertices());
			MaxBoneInfluences = IntCastChecked<int32>(SkinWeightVertexBuffer->GetMaxBoneInfluences());
			BoneIndexByteSize = IntCastChecked<uint32>(SkinWeightVertexBuffer->GetBoneIndexByteSize());
			BoneWeightByteSize = IntCastChecked<uint32>(SkinWeightVertexBuffer->GetBoneWeightByteSize());
			SkinWeightStride = SkinWeightVertexBuffer->GetConstantInfluencesVertexStride();
			SkinWeightBoneWeightsOffset = SkinWeightVertexBuffer->GetConstantInfluencesBoneWeightsOffset();
			bVariableBonesPerVertex = SkinWeightVertexBuffer->GetVariableBonesPerVertex();
			bUses16BitBoneIndex = SkinWeightVertexBuffer->Use16BitBoneIndex();
			bUses16BitBoneWeight = SkinWeightVertexBuffer->Use16BitBoneWeight();
			bHasSkinWeightBuffer = SkinWeightVertexCount > 0 && MaxBoneInfluences > 0;

			FPositionVertexBuffer* positionVertexBuffer = const_cast<FPositionVertexBuffer*>(
				&lodRenderData.StaticVertexBuffers.PositionVertexBuffer);
			FStaticMeshVertexBuffer* staticMeshVertexBuffer = const_cast<FStaticMeshVertexBuffer*>(
				&lodRenderData.StaticVertexBuffers.StaticMeshVertexBuffer);
			FColorVertexBuffer* colorVertexBuffer = const_cast<FColorVertexBuffer*>(
				&lodRenderData.StaticVertexBuffers.ColorVertexBuffer);
			FLocalVertexFactory* vertexFactoryPtr = &VertexFactory;
			FOptimizedSkeletalMeshVertexFactory* skinnedVertexFactoryPtr = &SkinnedVertexFactory;

			ENQUEUE_RENDER_COMMAND(InitOptimizedSkeletalMeshDirectVF)(
				[vertexFactoryPtr, skinnedVertexFactoryPtr, positionVertexBuffer, staticMeshVertexBuffer, colorVertexBuffer](FRHICommandList& InRHICmdList) {
					FLocalVertexFactory::FDataType data;
					positionVertexBuffer->BindPositionVertexBuffer(vertexFactoryPtr, data);
					staticMeshVertexBuffer->BindTangentVertexBuffer(vertexFactoryPtr, data);
					staticMeshVertexBuffer->BindPackedTexCoordVertexBuffer(vertexFactoryPtr, data);
					staticMeshVertexBuffer->BindLightMapVertexBuffer(vertexFactoryPtr, data, 0);
					colorVertexBuffer->BindColorVertexBuffer(vertexFactoryPtr, data);

					vertexFactoryPtr->SetData(InRHICmdList, data);
					vertexFactoryPtr->InitResource(InRHICmdList);

					FLocalVertexFactory::FDataType skinnedData = data;
					skinnedVertexFactoryPtr->SetData(InRHICmdList, skinnedData);
					skinnedVertexFactoryPtr->InitResource(InRHICmdList);
				});

			bInitialized = true;
		}

		bool CanUseGpuSkinning() const
		{
			return bInitialized && bHasSkinWeightBuffer && SkinWeightVertexBuffer;
		}

		FLocalVertexFactory VertexFactory;
		FOptimizedSkeletalMeshVertexFactory SkinnedVertexFactory;
		const FSkinWeightVertexBuffer* SkinWeightVertexBuffer = nullptr;
		int32 SkinWeightVertexCount = 0;
		int32 MaxBoneInfluences = 0;
		uint32 BoneIndexByteSize = 0;
		uint32 BoneWeightByteSize = 0;
		uint32 SkinWeightStride = 0;
		uint32 SkinWeightBoneWeightsOffset = 0;
		bool bVariableBonesPerVertex = false;
		bool bHasSkinWeightBuffer = false;
		bool bUses16BitBoneIndex = false;
		bool bUses16BitBoneWeight = false;
		bool bInitialized = false;
	};

	struct FLODResources
	{
		const FSkeletalMeshLODRenderData* LODRenderData = nullptr;
		TArray<FCachedSectionMesh> Sections;
		TUniquePtr<FDirectMeshResources> DirectResources;
		FColor debugColor = FColor::Yellow;

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
				OptimizedSkeletalMesh::FDirectMeshResources* releasedResources = DirectResources.Release();
				check(releasedResources);
			}
		}
	};

	struct FMeshRenderBatch
	{
		TObjectPtr<USkeletalMesh> skeletalMesh = nullptr;
		TArray<FRenderInstance> InInstances;
		TArray<TUniquePtr<FLODResources>> lodResources;
	};

	struct FVisibleLODInstances
	{
		TArray<const FRenderInstance*> InInstances;
	};

	static FRHIShaderResourceView* UploadUInt32DynamicBuffer(
		FMeshElementCollector& InCollector,
		TConstArrayView<uint32> InValues)
	{
		if (InValues.IsEmpty())
		{
			return nullptr;
		}

		FGlobalDynamicReadBuffer::FAllocation allocation = InCollector.GetDynamicReadBuffer().AllocateUInt32(InValues.Num());
		if (!allocation.IsValid())
		{
			return nullptr;
		}

		FMemory::Memcpy(allocation.Buffer, InValues.GetData(), InValues.Num() * sizeof(uint32));
		return allocation.SRV;
	}

	static void AddVisibleLODStat(FOptimizedSkeletalMeshRenderStats& InStats, const int32 lodIndex, const int32 InCount)
	{
		if (lodIndex < 0 || InCount <= 0)
		{
			return;
		}

		if (InStats.VisibleInstancesByLOD.Num() <= lodIndex)
		{
			InStats.VisibleInstancesByLOD.SetNumZeroed(lodIndex + 1);
		}

		InStats.VisibleInstancesByLOD[lodIndex] += InCount;
	}

	static int32 GetVisibleLODStat(const FOptimizedSkeletalMeshRenderStats& InStats, const int32 lodIndex)
	{
		return InStats.VisibleInstancesByLOD.IsValidIndex(lodIndex)
			? InStats.VisibleInstancesByLOD[lodIndex]
			: 0;
	}

	static const FSkeletalMeshLODRenderData* GetLODRenderData(const USkeletalMesh* skeletalMesh, int32 lodIndex);

	static FBox GetInstanceWorldBounds(const FOptimizedSkeletalMeshInstanceDesc& InDesc)
	{
		if (InDesc.SkeletalMesh)
		{
			FBox localRenderBounds(ForceInit);
			if (const FSkeletalMeshLODRenderData* lodRenderData = GetLODRenderData(InDesc.SkeletalMesh, 0))
			{
				const FPositionVertexBuffer& positionVertexBuffer = lodRenderData->StaticVertexBuffers.PositionVertexBuffer;
				const uint32 numVertices = positionVertexBuffer.GetNumVertices();
				for (uint32 vertexIndex = 0; vertexIndex < numVertices; ++vertexIndex)
				{
					localRenderBounds += static_cast<FVector>(positionVertexBuffer.VertexPosition(vertexIndex));
				}
			}

			if (localRenderBounds.IsValid)
			{
				return localRenderBounds.TransformBy(InDesc.WorldTransform);
			}

			return InDesc.SkeletalMesh->GetBounds().GetBox().TransformBy(InDesc.WorldTransform);
		}

		return FBox::BuildAABB(InDesc.WorldTransform.GetLocation(), FVector(FallbackInstanceExtent));
	}

	static bool DoesClipSpaceBoxIntersectView(const FSceneView& InView, const FBox& worldBounds)
	{
		if (!worldBounds.IsValid)
		{
			return false;
		}

		const FVector min = worldBounds.Min;
		const FVector max = worldBounds.Max;
		const FVector corners[] = {
			FVector(min.X, min.Y, min.Z),
			FVector(min.X, min.Y, max.Z),
			FVector(min.X, max.Y, min.Z),
			FVector(min.X, max.Y, max.Z),
			FVector(max.X, min.Y, min.Z),
			FVector(max.X, min.Y, max.Z),
			FVector(max.X, max.Y, min.Z),
			FVector(max.X, max.Y, max.Z),
		};

		const FMatrix& viewProjectionMatrix = InView.ViewMatrices.GetViewProjectionMatrix();
		bool bAllLeft = true;
		bool bAllRight = true;
		bool bAllBelow = true;
		bool bAllAbove = true;
		bool bAllBehindNear = true;

		for (const FVector& corner : corners)
		{
			const FVector4 clipPosition = viewProjectionMatrix.TransformFVector4(FVector4(corner, 1.0));
			const float clipW = static_cast<float>(clipPosition.W);

			if (clipW <= UE_SMALL_NUMBER)
			{
				bAllBehindNear = false;
				continue;
			}

			bAllLeft &= clipPosition.X < -clipW;
			bAllRight &= clipPosition.X > clipW;
			bAllBelow &= clipPosition.Y < -clipW;
			bAllAbove &= clipPosition.Y > clipW;
			bAllBehindNear = false;
		}

		return !(bAllLeft || bAllRight || bAllBelow || bAllAbove || bAllBehindNear);
	}

	static FBox GetCullTestBounds(const FRenderInstance& instance, const float InstanceCullBoundsScale)
	{
		if (!instance.worldBounds.IsValid)
		{
			return FBox(ForceInit);
		}

		return FBox::BuildAABB(
			instance.worldBounds.GetCenter(),
			instance.worldBounds.GetExtent() * FMath::Max(1.0f, InstanceCullBoundsScale));
	}

	static bool IsInstanceVisibleInView(
		const FSceneView& InView,
		const FRenderInstance& instance,
		const bool bEnableInstanceFrustumCulling,
		const float InstanceCullBoundsScale)
	{
		if (!bEnableInstanceFrustumCulling)
		{
			return true;
		}

		if (!instance.worldBounds.IsValid)
		{
			return false;
		}

		return DoesClipSpaceBoxIntersectView(InView, GetCullTestBounds(instance, InstanceCullBoundsScale));
	}

	static void BuildDynamicPrimitiveInstanceData(
		FMeshElementCollector& InCollector,
		TConstArrayView<const FRenderInstance*> InInstances,
		FMeshBatchDynamicPrimitiveData*& OutDynamicPrimitiveData,
		FBox& OutWorldBounds)
	{
		OutDynamicPrimitiveData = nullptr;
		OutWorldBounds = FBox(ForceInit);

		if (InInstances.IsEmpty())
		{
			return;
		}

		TArray<FInstanceSceneData>& instanceSceneData =
			InCollector.AllocateOneFrameResource<TArray<FInstanceSceneData>>();
		instanceSceneData.Reserve(InInstances.Num());

		for (const FRenderInstance* instance : InInstances)
		{
			if (!instance)
			{
				continue;
			}

			FInstanceSceneData& sceneData = instanceSceneData.AddDefaulted_GetRef();
			sceneData.LocalToPrimitive = FRenderTransform(instance->InLocalToWorld);
			OutWorldBounds += instance->worldBounds;
		}

		if (instanceSceneData.IsEmpty())
		{
			return;
		}

		FMeshBatchDynamicPrimitiveData& dynamicPrimitiveData =
			InCollector.AllocateOneFrameResource<FMeshBatchDynamicPrimitiveData>();
		dynamicPrimitiveData.InstanceSceneData = MakeArrayView(instanceSceneData);
		OutDynamicPrimitiveData = &dynamicPrimitiveData;
	}

	static FColor GetBatchDebugColor(const int32 InBatchIndex)
	{
		static const FColor colors[] = {
			FColor::Yellow,
			FColor::Cyan,
			FColor::Green,
			FColor::Orange,
			FColor::Magenta,
			FColor::Red,
			FColor::Blue,
			FColor::White,
		};

		return colors[InBatchIndex % UE_ARRAY_COUNT(colors)];
	}

	static const FMaterialRenderProxy* GetSectionMaterialRenderProxy(const USkeletalMesh* skeletalMesh, const FSkelMeshRenderSection& InSection)
	{
		const TArray<FSkeletalMaterial>& materials = skeletalMesh->GetMaterials();
		UMaterialInterface* material = materials.IsValidIndex(InSection.MaterialIndex)
			? materials[InSection.MaterialIndex].MaterialInterface
			: nullptr;

		if (!material)
		{
			material = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		return material ? material->GetRenderProxy() : nullptr;
	}

	static const FMaterialRenderProxy* GetWireframeMaterialRenderProxy()
	{
		if (GEngine && GEngine->WireframeMaterial)
		{
			return GEngine->WireframeMaterial->GetRenderProxy();
		}

		if (UMaterialInterface* defaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface))
		{
			return defaultMaterial->GetRenderProxy();
		}

		return nullptr;
	}

	static FLinearColor GetLODColorationColor(const int32 lodIndex)
	{
		if (GEngine && GEngine->LODColorationColors.IsValidIndex(lodIndex))
		{
			return GEngine->LODColorationColors[lodIndex];
		}

		return FLinearColor(GetBatchDebugColor(lodIndex));
	}

	static const FMaterialRenderProxy* GetLODColorationMaterialRenderProxy(
		FMeshElementCollector& InCollector,
		const int32 lodIndex)
	{
		const FMaterialRenderProxy* parentRenderProxy = nullptr;
		if (GEngine && GEngine->ShadedLevelColorationUnlitMaterial)
		{
			parentRenderProxy = GEngine->ShadedLevelColorationUnlitMaterial->GetRenderProxy();
		}
		else if (GEngine && GEngine->LevelColorationUnlitMaterial)
		{
			parentRenderProxy = GEngine->LevelColorationUnlitMaterial->GetRenderProxy();
		}
		else if (UMaterialInterface* defaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface))
		{
			parentRenderProxy = defaultMaterial->GetRenderProxy();
		}

		return parentRenderProxy
			? &InCollector.AllocateOneFrameResource<FColoredMaterialRenderProxy>(
				  parentRenderProxy,
				  GetLODColorationColor(lodIndex),
				  NAME_Color)
			: nullptr;
	}

	static void BuildCachedSectionMeshes(const USkeletalMesh* skeletalMesh, const int32 lodIndex, TArray<FCachedSectionMesh>& OutSections)
	{
		OutSections.Reset();

		if (!skeletalMesh)
		{
			return;
		}

		FSkeletalMeshRenderData* renderData = skeletalMesh->GetResourceForRendering();
		if (!renderData || !renderData->LODRenderData.IsValidIndex(lodIndex))
		{
			return;
		}

		const FSkeletalMeshLODRenderData& lodRenderData = renderData->LODRenderData[lodIndex];
		TArray<uint32> lodIndices;
		lodRenderData.MultiSizeIndexContainer.GetIndexBuffer(lodIndices);

		const uint32 numVertices = lodRenderData.GetNumVertices();
		const uint32 numTexCoords = lodRenderData.GetNumTexCoords();

		OutSections.Reserve(lodRenderData.RenderSections.Num());
		for (const FSkelMeshRenderSection& renderSection : lodRenderData.RenderSections)
		{
			if (!renderSection.IsValid())
			{
				continue;
			}

			FCachedSectionMesh& cachedSection = OutSections.AddDefaulted_GetRef();
			cachedSection.MaterialRenderProxy = GetSectionMaterialRenderProxy(skeletalMesh, renderSection);
			cachedSection.Vertices.Reserve(renderSection.NumVertices);
			cachedSection.Indices.Reserve(renderSection.NumTriangles * 3);

			TMap<uint32, int32> localVertexIndexByLODVertexIndex;
			localVertexIndexByLODVertexIndex.Reserve(renderSection.NumVertices);

			for (uint32 triangleIndex = 0; triangleIndex < renderSection.NumTriangles; ++triangleIndex)
			{
				for (uint32 cornerIndex = 0; cornerIndex < 3; ++cornerIndex)
				{
					const uint32 lodIndexBufferIndex = renderSection.BaseIndex + triangleIndex * 3 + cornerIndex;
					if (!lodIndices.IsValidIndex(lodIndexBufferIndex))
					{
						continue;
					}

					const uint32 lodVertexIndex = lodIndices[lodIndexBufferIndex];
					if (lodVertexIndex >= numVertices)
					{
						continue;
					}

					int32* existingLocalVertexIndex = localVertexIndexByLODVertexIndex.Find(lodVertexIndex);
					if (!existingLocalVertexIndex)
					{
						FDynamicMeshVertex vertex;
						vertex.Position = lodRenderData.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(lodVertexIndex);
						vertex.TangentX = lodRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentX(lodVertexIndex);
						vertex.TangentZ = lodRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(lodVertexIndex);
						vertex.Color = FColor::White;

						const FVector2f uv = numTexCoords > 0
							? lodRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV(lodVertexIndex, 0)
							: FVector2f::ZeroVector;

						for (int32 uvIndex = 0; uvIndex < MAX_STATIC_TEXCOORDS; ++uvIndex)
						{
							vertex.TextureCoordinate[uvIndex] = uv;
						}

						const int32 newLocalVertexIndex = cachedSection.Vertices.Add(vertex);
						localVertexIndexByLODVertexIndex.Add(lodVertexIndex, newLocalVertexIndex);
						existingLocalVertexIndex = localVertexIndexByLODVertexIndex.Find(lodVertexIndex);
					}

					cachedSection.Indices.Add(*existingLocalVertexIndex);
				}
			}
		}
	}

	static const FSkeletalMeshLODRenderData* GetLODRenderData(const USkeletalMesh* skeletalMesh, const int32 lodIndex)
	{
		if (!skeletalMesh)
		{
			return nullptr;
		}

		const FSkeletalMeshRenderData* renderData = skeletalMesh->GetResourceForRendering();
		if (!renderData || !renderData->LODRenderData.IsValidIndex(lodIndex))
		{
			return nullptr;
		}

		return &renderData->LODRenderData[lodIndex];
	}

	static int32 GetRenderLODCount(const USkeletalMesh* skeletalMesh)
	{
		if (!skeletalMesh)
		{
			return 0;
		}

		const FSkeletalMeshRenderData* renderData = skeletalMesh->GetResourceForRendering();
		return renderData ? renderData->LODRenderData.Num() : 0;
	}

	static float GetLODScreenSizeThreshold(const USkeletalMesh* skeletalMesh, const int32 lodIndex)
	{
		if (skeletalMesh)
		{
			if (const FSkeletalMeshLODInfo* LODInfo = skeletalMesh->GetLODInfo(lodIndex))
			{
				return LODInfo->ScreenSize.Default;
			}
		}

		return FMath::Pow(0.5f, static_cast<float>(lodIndex));
	}

	static int32 ChooseLODForView(
		const FSceneView& InView,
		const USkeletalMesh* skeletalMesh,
		const FRenderInstance& instance,
		const int32 InNumLODs)
	{
		if (InNumLODs <= 1)
		{
			return 0;
		}

		if (!instance.bAutoLOD)
		{
			return FMath::Clamp(instance.ForcedLODIndex, 0, InNumLODs - 1);
		}

		const FBoxSphereBounds bounds(instance.worldBounds);
		const float screenSize = ComputeBoundsScreenSize(
			FVector4(bounds.Origin, 1.0f),
			bounds.SphereRadius,
			InView);

		int32 chosenLOD = 0;
		for (int32 lodIndex = 1; lodIndex < InNumLODs; ++lodIndex)
		{
			if (screenSize <= GetLODScreenSizeThreshold(skeletalMesh, lodIndex))
			{
				chosenLOD = lodIndex;
			}
		}

		return FMath::Clamp(chosenLOD, 0, InNumLODs - 1);
	}
} // namespace OptimizedSkeletalMesh

class FOptimizedSkeletalMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	explicit FOptimizedSkeletalMeshSceneProxy(const UOptimizedSkeletalMeshRenderComponent* InComponent)
		: FPrimitiveSceneProxy(InComponent)
		, StatsComponent(const_cast<UOptimizedSkeletalMeshRenderComponent*>(InComponent))
		, bDrawDebugBounds(InComponent->ShouldDrawDebugBounds())
		, bDrawMeshSections(InComponent->ShouldDrawMeshSections())
		, MeshDrawMode(InComponent->GetMeshDrawMode())
		, MaxMeshDrawInstances(InComponent->GetMaxMeshDrawInstances())
		, bEnableInstanceFrustumCulling(InComponent->ShouldEnableInstanceFrustumCulling())
		, InstanceCullBoundsScale(InComponent->GetInstanceCullBoundsScale())
		, bDrawCullingDebug(InComponent->ShouldDrawCullingDebug())
		, bDrawCullTestBounds(InComponent->ShouldDrawCullTestBounds())
	{
		if (const UWorld* world = InComponent->GetWorld())
		{
			if (const UOptimizedSkeletalMeshWorldSubsystem* Subsystem = world->GetSubsystem<UOptimizedSkeletalMeshWorldSubsystem>())
			{
				TArray<FOptimizedSkeletalMeshInstanceSnapshot> snapshots;
				Subsystem->GetInstancesSnapshot(snapshots);

				TMap<USkeletalMesh*, int32> batchIndexByMesh;
				MeshBatches.Reserve(snapshots.Num());

				for (const FOptimizedSkeletalMeshInstanceSnapshot& snapshot : snapshots)
				{
					if (!snapshot.Desc.bVisible || !snapshot.Desc.SkeletalMesh)
					{
						continue;
					}

					USkeletalMesh* skeletalMesh = snapshot.Desc.SkeletalMesh.Get();
					int32* existingBatchIndex = batchIndexByMesh.Find(skeletalMesh);

					if (!existingBatchIndex)
					{
						const int32 newBatchIndex = MeshBatches.Num();
						OptimizedSkeletalMesh::FMeshRenderBatch& newBatch = MeshBatches.AddDefaulted_GetRef();
						newBatch.skeletalMesh = skeletalMesh;
						batchIndexByMesh.Add(skeletalMesh, newBatchIndex);
						existingBatchIndex = batchIndexByMesh.Find(skeletalMesh);
					}

					OptimizedSkeletalMesh::FMeshRenderBatch& batch = MeshBatches[*existingBatchIndex];
					OptimizedSkeletalMesh::FRenderInstance& renderInstance = batch.InInstances.AddDefaulted_GetRef();
					renderInstance.InstanceId = snapshot.Handle.Id;
					renderInstance.worldBounds = OptimizedSkeletalMesh::GetInstanceWorldBounds(snapshot.Desc);
					renderInstance.InLocalToWorld = FMatrix44f(snapshot.Desc.WorldTransform.ToMatrixWithScale());
					renderInstance.ForcedLODIndex = FMath::Max(0, snapshot.Desc.LODIndex);
					renderInstance.bAutoLOD = snapshot.Desc.bAutoLOD;
					++RegisteredInstanceCount;
				}

				for (OptimizedSkeletalMesh::FMeshRenderBatch& batch : MeshBatches)
				{
					const int32 lodCount = OptimizedSkeletalMesh::GetRenderLODCount(batch.skeletalMesh);
					batch.lodResources.Reserve(lodCount);
					for (int32 lodIndex = 0; lodIndex < lodCount; ++lodIndex)
					{
						TUniquePtr<OptimizedSkeletalMesh::FLODResources> lodResources =
							MakeUnique<OptimizedSkeletalMesh::FLODResources>();
						lodResources->LODRenderData = OptimizedSkeletalMesh::GetLODRenderData(batch.skeletalMesh, lodIndex);
						lodResources->debugColor = OptimizedSkeletalMesh::GetBatchDebugColor(lodIndex);

						if (bDrawMeshSections)
						{
							if (MeshDrawMode == EOptimizedSkeletalMeshDrawMode::DynamicMeshProof)
							{
								OptimizedSkeletalMesh::BuildCachedSectionMeshes(
									batch.skeletalMesh,
									lodIndex,
									lodResources->Sections);
							}
							else if (lodResources->LODRenderData)
							{
								lodResources->DirectResources =
									MakeUnique<OptimizedSkeletalMesh::FDirectMeshResources>(GetScene().GetFeatureLevel());
								lodResources->DirectResources->Init(*lodResources->LODRenderData);

								if (lodResources->DirectResources->bHasSkinWeightBuffer)
								{
									++SkinningSkinWeightLODs;
									SkinningSkinWeightVertices += lodResources->DirectResources->SkinWeightVertexCount;
									SkinningMaxBoneInfluences = FMath::Max(
										SkinningMaxBoneInfluences,
										lodResources->DirectResources->MaxBoneInfluences);

									if (lodResources->DirectResources->bUses16BitBoneIndex)
									{
										++SkinningSkinWeight16BitIndexLODs;
									}

									if (lodResources->DirectResources->bUses16BitBoneWeight)
									{
										++SkinningSkinWeight16BitWeightLODs;
									}

									if (lodResources->DirectResources->CanUseGpuSkinning())
									{
										++SkinningGPUSkinReadyLODs;
									}
								}
								else
								{
									++SkinningMissingSkinWeightLODs;
								}
							}
						}

						batch.lodResources.Add(MoveTemp(lodResources));
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
		const TArray<const FSceneView*>& InViews,
		const FSceneViewFamily& InViewFamily,
		uint32 InVisibilityMap,
		FMeshElementCollector& InCollector) const override
	{
		FRHICommandList& rhiCmdList = InCollector.GetRHICommandList();
		RHI_BREADCRUMB_EVENT_STAT(rhiCmdList, OSMRender, "OptimizedSkeletalMesh");
		SCOPED_DRAW_EVENT(rhiCmdList, OptimizedSkeletalMesh);

		for (int32 viewIndex = 0; viewIndex < InViews.Num(); ++viewIndex)
		{
			if (!(InVisibilityMap & (1 << viewIndex)))
			{
				continue;
			}

			FOptimizedSkeletalMeshRenderStats frameStats;
			frameStats.FrameNumber = static_cast<int32>(GFrameCounter);
			frameStats.RegisteredInstances = RegisteredInstanceCount;
			frameStats.MeshBatches = MeshBatches.Num();
			frameStats.SkinningPaletteInstances = SkinningPaletteInstances;
			frameStats.SkinningPaletteMatrices = SkinningPaletteMatrices;
			frameStats.SkinningPaletteBytes = SkinningPaletteBytes;
			frameStats.SkinningGPUPaletteMatrices = SkinningGPUPaletteMatrices;
			frameStats.SkinningGPUPaletteBytes = SkinningGPUPaletteBytes;
			frameStats.SkinningGPUPaletteUploads = SkinningGPUPaletteUploads;
			frameStats.SkinningSkinWeightLODs = SkinningSkinWeightLODs;
			frameStats.SkinningSkinWeightVertices = SkinningSkinWeightVertices;
			frameStats.SkinningMaxBoneInfluences = SkinningMaxBoneInfluences;
			frameStats.SkinningSkinWeight16BitIndexLODs = SkinningSkinWeight16BitIndexLODs;
			frameStats.SkinningSkinWeight16BitWeightLODs = SkinningSkinWeight16BitWeightLODs;
			frameStats.SkinningMissingSkinWeightLODs = SkinningMissingSkinWeightLODs;
			frameStats.SkinningGPUSkinReadyLODs = SkinningGPUSkinReadyLODs;

			const bool bIsWireframeView = InViewFamily.EngineShowFlags.Wireframe;
			const bool bIsLODColorationView = InViewFamily.EngineShowFlags.LODColoration;
			FPrimitiveDrawInterface* pdi = InCollector.GetPDI(viewIndex);
			int32 drawnMeshInstances = 0;

			for (const OptimizedSkeletalMesh::FMeshRenderBatch& batch : MeshBatches)
			{
				if (bDrawMeshSections
					&& (MeshDrawMode == EOptimizedSkeletalMeshDrawMode::DirectMeshInstanced
						|| MeshDrawMode == EOptimizedSkeletalMeshDrawMode::GpuSkinnedInstanced))
				{
					TArray<OptimizedSkeletalMesh::FVisibleLODInstances> visibleInstancesByLod;
					visibleInstancesByLod.SetNum(batch.lodResources.Num());

					for (const OptimizedSkeletalMesh::FRenderInstance& instance : batch.InInstances)
					{
						++frameStats.TestedInstances;
						const bool bInstanceVisibleInView = OptimizedSkeletalMesh::IsInstanceVisibleInView(
							*InViews[viewIndex],
							instance,
							bEnableInstanceFrustumCulling,
							InstanceCullBoundsScale);

						if (bDrawCullingDebug)
						{
							DrawWireBox(
								pdi,
								instance.worldBounds,
								bInstanceVisibleInView ? FColor::Green : FColor::Red,
								SDPG_Foreground);

							if (bDrawCullTestBounds)
							{
								const FBox cullTestBounds = OptimizedSkeletalMesh::GetCullTestBounds(instance, InstanceCullBoundsScale);
								if (cullTestBounds.IsValid)
								{
									DrawWireBox(
										pdi,
										cullTestBounds,
										bInstanceVisibleInView ? FColor::Cyan : FColor::Orange,
										SDPG_Foreground);
								}
							}
						}

						if (!bInstanceVisibleInView || (MaxMeshDrawInstances > 0 && drawnMeshInstances >= MaxMeshDrawInstances))
						{
							if (!bInstanceVisibleInView)
							{
								++frameStats.CulledInstances;
							}
							continue;
						}

						++frameStats.VisibleInstances;
						if (MeshDrawMode == EOptimizedSkeletalMeshDrawMode::GpuSkinnedInstanced)
						{
							frameStats.RenderVisibleInstanceIds.AddUnique(instance.InstanceId);
						}
						const int32 chosenLodIndex = OptimizedSkeletalMesh::ChooseLODForView(
							*InViews[viewIndex],
							batch.skeletalMesh,
							instance,
							batch.lodResources.Num());

						if (!visibleInstancesByLod.IsValidIndex(chosenLodIndex))
						{
							continue;
						}

						visibleInstancesByLod[chosenLodIndex].InInstances.Add(&instance);
						++drawnMeshInstances;
						++frameStats.DrawnInstances;
						OptimizedSkeletalMesh::AddVisibleLODStat(frameStats, chosenLodIndex, 1);

						if (bDrawDebugBounds && !bDrawCullingDebug)
						{
							const OptimizedSkeletalMesh::FLODResources* lodResources =
								batch.lodResources.IsValidIndex(chosenLodIndex)
								? batch.lodResources[chosenLodIndex].Get()
								: nullptr;
							const FColor debugColor = lodResources
								? lodResources->debugColor
								: OptimizedSkeletalMesh::GetBatchDebugColor(chosenLodIndex);
							DrawWireBox(pdi, instance.worldBounds, debugColor, SDPG_Foreground);
						}
					}

					for (int32 lodIndex = 0; lodIndex < visibleInstancesByLod.Num(); ++lodIndex)
					{
						const OptimizedSkeletalMesh::FLODResources* lodResources =
							batch.lodResources.IsValidIndex(lodIndex)
							? batch.lodResources[lodIndex].Get()
							: nullptr;
						if (!lodResources || !lodResources->LODRenderData || !lodResources->DirectResources || !lodResources->DirectResources->bInitialized)
						{
							continue;
						}

						const TArray<const OptimizedSkeletalMesh::FRenderInstance*>& visibleInstances =
							visibleInstancesByLod[lodIndex].InInstances;
						if (visibleInstances.IsEmpty())
						{
							continue;
						}

						FMeshBatchDynamicPrimitiveData* dynamicPrimitiveData = nullptr;
						FBox worldBounds(ForceInit);
						OptimizedSkeletalMesh::BuildDynamicPrimitiveInstanceData(
							InCollector,
							MakeArrayView(visibleInstances),
							dynamicPrimitiveData,
							worldBounds);
						if (!dynamicPrimitiveData || !worldBounds.IsValid)
						{
							continue;
						}

						const bool bUseGpuSkinningForLOD =
							MeshDrawMode == EOptimizedSkeletalMeshDrawMode::GpuSkinnedInstanced
							&& lodResources->DirectResources->CanUseGpuSkinning();
						if (MeshDrawMode == EOptimizedSkeletalMeshDrawMode::GpuSkinnedInstanced && !bUseGpuSkinningForLOD)
						{
							frameStats.SkinningGPUSkinFallbackDraws += visibleInstances.Num();
						}

						TArray<uint32> instancePaletteOffsets;
						FRHIShaderResourceView* instancePaletteOffsetSRV = nullptr;
						if (bUseGpuSkinningForLOD)
						{
							instancePaletteOffsets.Reserve(visibleInstances.Num());
							for (const OptimizedSkeletalMesh::FRenderInstance* visibleInstance : visibleInstances)
							{
								const OptimizedSkeletalMesh::FBonePaletteRange* range =
									visibleInstance ? BonePaletteRangesByInstanceId.Find(visibleInstance->InstanceId) : nullptr;
								instancePaletteOffsets.Add(range ? range->Offset : 0u);
								if (!range)
								{
									++frameStats.SkinningGPUSkinFallbackDraws;
								}
							}

							instancePaletteOffsetSRV = OptimizedSkeletalMesh::UploadUInt32DynamicBuffer(
								InCollector,
								MakeArrayView(instancePaletteOffsets));
							frameStats.SkinningGPUInstanceOffsetEntries += instancePaletteOffsets.Num();
						}

						FDynamicPrimitiveUniformBuffer& dynamicPrimitiveUniformBuffer =
							InCollector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
						const FMatrix primitiveLocalToWorld = FMatrix::Identity;
						dynamicPrimitiveUniformBuffer.Set(
							InCollector.GetRHICommandList(),
							primitiveLocalToWorld,
							primitiveLocalToWorld,
							worldBounds,
							GetLocalBounds(),
							true,
							false,
							false);

						for (const FSkelMeshRenderSection& renderSection : lodResources->LODRenderData->RenderSections)
						{
							if (!renderSection.IsValid())
							{
								continue;
							}

							const FMaterialRenderProxy* MaterialRenderProxy = bIsWireframeView
								? OptimizedSkeletalMesh::GetWireframeMaterialRenderProxy()
								: OptimizedSkeletalMesh::GetSectionMaterialRenderProxy(batch.skeletalMesh, renderSection);
							if (!MaterialRenderProxy)
							{
								continue;
							}

							FOptimizedSkeletalMeshVertexFactoryUserData* skinnedFactoryUserData = nullptr;
							if (bUseGpuSkinningForLOD)
							{
								TArray<uint32>& sectionBoneMap = InCollector.AllocateOneFrameResource<TArray<uint32>>();
								sectionBoneMap.Reserve(renderSection.BoneMap.Num());
								for (const FBoneIndexType boneIndex : renderSection.BoneMap)
								{
									sectionBoneMap.Add(static_cast<uint32>(boneIndex));
								}

								skinnedFactoryUserData =
									&InCollector.AllocateOneFrameResource<FOptimizedSkeletalMeshVertexFactoryUserData>();
								skinnedFactoryUserData->SkinWeightDataSRV =
									lodResources->DirectResources->SkinWeightVertexBuffer->GetDataVertexBuffer()->GetSRV();
								skinnedFactoryUserData->SkinWeightLookupSRV =
									lodResources->DirectResources->SkinWeightVertexBuffer->GetLookupVertexBuffer()->GetSRV();
								skinnedFactoryUserData->BonePaletteSRV =
									BonePalettePooledBuffer
									? const_cast<FRDGPooledBuffer*>(BonePalettePooledBuffer.GetReference())->GetSRV()
									: nullptr;
								skinnedFactoryUserData->InstancePaletteOffsetSRV = instancePaletteOffsetSRV;
								skinnedFactoryUserData->SectionBoneMapSRV = OptimizedSkeletalMesh::UploadUInt32DynamicBuffer(
									InCollector,
									MakeArrayView(sectionBoneMap));
								skinnedFactoryUserData->BonePaletteMatrixCount =
									BonePalettePooledBuffer ? static_cast<uint32>(SkinningGPUPaletteMatrices) : 0u;
								skinnedFactoryUserData->MaxBoneInfluences =
									static_cast<uint32>(lodResources->DirectResources->MaxBoneInfluences);
								skinnedFactoryUserData->BoneIndexByteSize = lodResources->DirectResources->BoneIndexByteSize;
								skinnedFactoryUserData->BoneWeightByteSize = lodResources->DirectResources->BoneWeightByteSize;
								skinnedFactoryUserData->SkinWeightStride = lodResources->DirectResources->SkinWeightStride;
								skinnedFactoryUserData->SkinWeightBoneWeightsOffset =
									lodResources->DirectResources->SkinWeightBoneWeightsOffset;
								skinnedFactoryUserData->InstancePaletteOffsetCount = instancePaletteOffsets.Num();
								skinnedFactoryUserData->SectionBoneMapCount = sectionBoneMap.Num();
								skinnedFactoryUserData->bVariableBonesPerVertex =
									lodResources->DirectResources->bVariableBonesPerVertex ? 1u : 0u;
								++frameStats.SkinningGPUSkinDraws;
								frameStats.SkinningGPUSectionBoneMapEntries += sectionBoneMap.Num();
							}

							FMeshBatch& mesh = InCollector.AllocateMesh();
							mesh.VertexFactory =
								bUseGpuSkinningForLOD
								? static_cast<const FVertexFactory*>(&lodResources->DirectResources->SkinnedVertexFactory)
								: static_cast<const FVertexFactory*>(&lodResources->DirectResources->VertexFactory);
							mesh.MaterialRenderProxy = MaterialRenderProxy;
							mesh.ReverseCulling = false;
							mesh.Type = PT_TriangleList;
							mesh.DepthPriorityGroup = SDPG_World;
							mesh.bCanApplyViewModeOverrides = true;
							mesh.CastShadow = false;
							mesh.bWireframe = bIsWireframeView;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
							mesh.VisualizeLODIndex = lodIndex;
#endif

							FMeshBatchElement& batchElement = mesh.Elements[0];
							batchElement.IndexBuffer = lodResources->LODRenderData->MultiSizeIndexContainer.GetIndexBuffer();
							batchElement.FirstIndex = renderSection.BaseIndex;
							batchElement.NumPrimitives = renderSection.NumTriangles;
							batchElement.MinVertexIndex = renderSection.BaseVertexIndex;
							batchElement.MaxVertexIndex = renderSection.BaseVertexIndex + renderSection.NumVertices - 1;
							batchElement.NumInstances = IntCastChecked<uint32>(visibleInstances.Num());
							batchElement.DynamicPrimitiveData = dynamicPrimitiveData;
							batchElement.UserData = skinnedFactoryUserData;
							batchElement.PrimitiveUniformBufferResource = &dynamicPrimitiveUniformBuffer.UniformBuffer;

							InCollector.AddMesh(viewIndex, mesh);
							++frameStats.SubmittedDrawCalls;
							++frameStats.SubmittedSections;
							frameStats.SubmittedTriangles += renderSection.NumTriangles;
						}
					}

					continue;
				}

				for (const OptimizedSkeletalMesh::FRenderInstance& instance : batch.InInstances)
				{
					++frameStats.TestedInstances;
					const bool bInstanceVisibleInView = OptimizedSkeletalMesh::IsInstanceVisibleInView(
						*InViews[viewIndex],
						instance,
						bEnableInstanceFrustumCulling,
						InstanceCullBoundsScale);

					if (bDrawCullingDebug)
					{
						DrawWireBox(
							pdi,
							instance.worldBounds,
							bInstanceVisibleInView ? FColor::Green : FColor::Red,
							SDPG_Foreground);

						if (bDrawCullTestBounds)
						{
							const FBox cullTestBounds = OptimizedSkeletalMesh::GetCullTestBounds(instance, InstanceCullBoundsScale);
							if (cullTestBounds.IsValid)
							{
								DrawWireBox(
									pdi,
									cullTestBounds,
									bInstanceVisibleInView ? FColor::Cyan : FColor::Orange,
									SDPG_Foreground);
							}
						}
					}

					if (!bInstanceVisibleInView)
					{
						++frameStats.CulledInstances;
						continue;
					}

					const int32 chosenLodIndex = OptimizedSkeletalMesh::ChooseLODForView(
						*InViews[viewIndex],
						batch.skeletalMesh,
						instance,
						batch.lodResources.Num());
					++frameStats.VisibleInstances;
					const OptimizedSkeletalMesh::FLODResources* lodResources =
						batch.lodResources.IsValidIndex(chosenLodIndex)
						? batch.lodResources[chosenLodIndex].Get()
						: nullptr;

					if (bDrawMeshSections && lodResources && (MaxMeshDrawInstances <= 0 || drawnMeshInstances < MaxMeshDrawInstances))
					{
						++frameStats.DrawnInstances;
						if (MeshDrawMode == EOptimizedSkeletalMeshDrawMode::GpuSkinnedInstanced)
						{
							frameStats.RenderVisibleInstanceIds.AddUnique(instance.InstanceId);
						}
						OptimizedSkeletalMesh::AddVisibleLODStat(frameStats, chosenLodIndex, 1);
						if (MeshDrawMode == EOptimizedSkeletalMeshDrawMode::DynamicMeshProof)
						{
							for (const OptimizedSkeletalMesh::FCachedSectionMesh& InSection : lodResources->Sections)
							{
								if (!InSection.MaterialRenderProxy || InSection.Vertices.IsEmpty() || InSection.Indices.IsEmpty())
								{
									continue;
								}

								FDynamicMeshBuilder meshBuilder(InViews[viewIndex]->GetFeatureLevel());
								meshBuilder.ReserveVertices(InSection.Vertices.Num());
								meshBuilder.ReserveTriangles(InSection.Indices.Num() / 3);
								meshBuilder.AddVertices(InSection.Vertices);
								meshBuilder.AddTriangles(InSection.Indices);
								FDynamicMeshBuilderSettings meshSettings;
								meshSettings.bWireframe = bIsWireframeView;
								meshSettings.bDisableBackfaceCulling = false;
								meshSettings.bReceivesDecals = true;
								meshSettings.bUseSelectionOutline = true;
								meshSettings.bCanApplyViewModeOverrides = true;
								const FMaterialRenderProxy* dynamicMaterialRenderProxy = bIsLODColorationView
									? OptimizedSkeletalMesh::GetLODColorationMaterialRenderProxy(InCollector, chosenLodIndex)
									: bIsWireframeView
									? OptimizedSkeletalMesh::GetWireframeMaterialRenderProxy()
									: InSection.MaterialRenderProxy;
								if (!dynamicMaterialRenderProxy)
								{
									continue;
								}

								meshBuilder.GetMesh(
									FMatrix(instance.InLocalToWorld),
									dynamicMaterialRenderProxy,
									SDPG_World,
									meshSettings,
									nullptr,
									viewIndex,
									InCollector);
								++frameStats.SubmittedDrawCalls;
								++frameStats.SubmittedSections;
								frameStats.SubmittedTriangles += InSection.Indices.Num() / 3;
							}
						}
						else if (lodResources->LODRenderData && lodResources->DirectResources && lodResources->DirectResources->bInitialized)
						{
							for (const FSkelMeshRenderSection& renderSection : lodResources->LODRenderData->RenderSections)
							{
								if (!renderSection.IsValid())
								{
									continue;
								}

								const FMaterialRenderProxy* MaterialRenderProxy = bIsWireframeView
									? OptimizedSkeletalMesh::GetWireframeMaterialRenderProxy()
									: OptimizedSkeletalMesh::GetSectionMaterialRenderProxy(batch.skeletalMesh, renderSection);
								if (!MaterialRenderProxy)
								{
									continue;
								}

								FMeshBatch& mesh = InCollector.AllocateMesh();
								mesh.VertexFactory = &lodResources->DirectResources->VertexFactory;
								mesh.MaterialRenderProxy = MaterialRenderProxy;
								mesh.ReverseCulling = FMatrix(instance.InLocalToWorld).Determinant() < 0.0;
								mesh.Type = PT_TriangleList;
								mesh.DepthPriorityGroup = SDPG_World;
								mesh.bCanApplyViewModeOverrides = true;
								mesh.CastShadow = false;
								mesh.bWireframe = bIsWireframeView;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
								mesh.VisualizeLODIndex = chosenLodIndex;
#endif

								FMeshBatchElement& batchElement = mesh.Elements[0];
								batchElement.IndexBuffer = lodResources->LODRenderData->MultiSizeIndexContainer.GetIndexBuffer();
								batchElement.FirstIndex = renderSection.BaseIndex;
								batchElement.NumPrimitives = renderSection.NumTriangles;
								batchElement.MinVertexIndex = renderSection.BaseVertexIndex;
								batchElement.MaxVertexIndex = renderSection.BaseVertexIndex + renderSection.NumVertices - 1;
								FDynamicPrimitiveUniformBuffer& dynamicPrimitiveUniformBuffer =
									InCollector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
								const FMatrix instanceLocalToWorld(instance.InLocalToWorld);
								dynamicPrimitiveUniformBuffer.Set(
									InCollector.GetRHICommandList(),
									instanceLocalToWorld,
									instanceLocalToWorld,
									instance.worldBounds,
									GetLocalBounds(),
									true,
									false,
									false);
								batchElement.PrimitiveUniformBufferResource = &dynamicPrimitiveUniformBuffer.UniformBuffer;

								InCollector.AddMesh(viewIndex, mesh);
								++frameStats.SubmittedDrawCalls;
								++frameStats.SubmittedSections;
								frameStats.SubmittedTriangles += renderSection.NumTriangles;
							}
						}

						++drawnMeshInstances;
					}

					if (bDrawDebugBounds && !bDrawCullingDebug)
					{
						const FColor debugColor = lodResources
							? lodResources->debugColor
							: OptimizedSkeletalMesh::GetBatchDebugColor(chosenLodIndex);
						DrawWireBox(pdi, instance.worldBounds, debugColor, SDPG_Foreground);
					}
				}
			}

			PublishRenderStats(frameStats);
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* InView) const override
	{
		FPrimitiveViewRelevance result;
		result.bDrawRelevance = IsShown(InView);
		result.bDynamicRelevance = true;
		result.bOpaque = bDrawMeshSections;
		result.bShadowRelevance = false;
		result.bEditorPrimitiveRelevance = UseEditorCompositing(InView);
		return result;
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetOptimizedAllocatedSize();
	}

	void PublishRenderStats(const FOptimizedSkeletalMeshRenderStats& InStats) const
	{
		if (!StatsComponent.IsValid())
		{
			return;
		}

		const TWeakObjectPtr<UOptimizedSkeletalMeshRenderComponent> weakStatsComponent = StatsComponent;
		AsyncTask(
			ENamedThreads::GameThread,
			[weakStatsComponent, InStats]() {
				if (UOptimizedSkeletalMeshRenderComponent* InComponent = weakStatsComponent.Get())
				{
					InComponent->ApplyRenderStats_GameThread(InStats);
				}
			});
	}

	void UpdateBonePalettes_RenderThread(
		FRHICommandListImmediate& InRHICmdList,
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

		for (OptimizedSkeletalMesh::FBonePaletteRenderSnapshot& snapshot : InSnapshots)
		{
			const int32 boneCount = snapshot.BonePalette.Num();
			if (boneCount <= 0)
			{
				continue;
			}

			const uint32 paletteOffset = static_cast<uint32>(PackedBonePalettes.Num());
			OptimizedSkeletalMesh::FBonePaletteRange range;
			range.Offset = paletteOffset;
			range.boneCount = static_cast<uint32>(boneCount);
			BonePaletteRangesByInstanceId.Add(snapshot.InstanceId, range);

			PackedBonePalettes.Append(snapshot.BonePalette);
			SkinningPaletteMatrices += boneCount;
			SkinningPaletteBytes += boneCount * sizeof(FMatrix44f);
			BonePalettesByInstanceId.Add(snapshot.InstanceId, MoveTemp(snapshot.BonePalette));
		}

		SkinningPaletteInstances = BonePalettesByInstanceId.Num();
		UploadBonePaletteBuffer_RenderThread(InRHICmdList);
	}

	void UploadBonePaletteBuffer_RenderThread(FRHICommandListImmediate& InRHICmdList)
	{
		check(IsInRenderingThread());

		if (PackedBonePalettes.IsEmpty())
		{
			BonePalettePooledBuffer.SafeRelease();
			SkinningGPUPaletteMatrices = 0;
			SkinningGPUPaletteBytes = 0;
			return;
		}

		FRDGBuilder graphBuilder(
			InRHICmdList,
			RDG_EVENT_NAME("OptimizedSkeletalMesh.UploadBonePalettes"));

		FRDGBufferRef bonePaletteBuffer = CreateStructuredBuffer(
			graphBuilder,
			TEXT("OptimizedSkeletalMesh.bonePaletteBuffer"),
			static_cast<const TArray<FMatrix44f>&>(PackedBonePalettes));

		graphBuilder.QueueBufferExtraction(
			bonePaletteBuffer,
			&BonePalettePooledBuffer,
			ERHIAccess::SRVMask);
		graphBuilder.Execute();

		++SkinningGPUPaletteUploads;
		SkinningGPUPaletteMatrices = PackedBonePalettes.Num();
		SkinningGPUPaletteBytes = PackedBonePalettes.Num() * sizeof(FMatrix44f);
	}

	uint32 GetOptimizedAllocatedSize() const
	{
		SIZE_T allocatedSize = FPrimitiveSceneProxy::GetAllocatedSize() + MeshBatches.GetAllocatedSize();
		for (const OptimizedSkeletalMesh::FMeshRenderBatch& batch : MeshBatches)
		{
			allocatedSize += batch.InInstances.GetAllocatedSize();
			allocatedSize += batch.lodResources.GetAllocatedSize();
			for (const TUniquePtr<OptimizedSkeletalMesh::FLODResources>& lodResources : batch.lodResources)
			{
				if (!lodResources)
				{
					continue;
				}

				allocatedSize += sizeof(OptimizedSkeletalMesh::FLODResources);
				allocatedSize += lodResources->Sections.GetAllocatedSize();
				for (const OptimizedSkeletalMesh::FCachedSectionMesh& InSection : lodResources->Sections)
				{
					allocatedSize += InSection.Vertices.GetAllocatedSize();
					allocatedSize += InSection.Indices.GetAllocatedSize();
				}
			}
		}

		allocatedSize += BonePalettesByInstanceId.GetAllocatedSize();
		for (const TPair<int32, TArray<FMatrix44f>>& pair : BonePalettesByInstanceId)
		{
			allocatedSize += pair.Value.GetAllocatedSize();
		}
		allocatedSize += BonePaletteRangesByInstanceId.GetAllocatedSize();
		allocatedSize += PackedBonePalettes.GetAllocatedSize();

		return IntCastChecked<uint32>(allocatedSize);
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
	int32 SkinningSkinWeightLODs = 0;
	int32 SkinningSkinWeightVertices = 0;
	int32 SkinningMaxBoneInfluences = 0;
	int32 SkinningSkinWeight16BitIndexLODs = 0;
	int32 SkinningSkinWeight16BitWeightLODs = 0;
	int32 SkinningMissingSkinWeightLODs = 0;
	int32 SkinningGPUSkinReadyLODs = 0;
	bool bDrawDebugBounds = true;
	bool bDrawMeshSections = false;
	EOptimizedSkeletalMeshDrawMode MeshDrawMode = EOptimizedSkeletalMeshDrawMode::DynamicMeshProof;
	int32 MaxMeshDrawInstances = 0;
	bool bEnableInstanceFrustumCulling = true;
	float InstanceCullBoundsScale = 1.5f;
	bool bDrawCullingDebug = false;
	bool bDrawCullTestBounds = true;
};

UOptimizedSkeletalMeshRenderComponent::UOptimizedSkeletalMeshRenderComponent(const FObjectInitializer& InObjectInitializer)
	: Super(InObjectInitializer)
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

bool UOptimizedSkeletalMeshRenderComponent::PushBonePalettesToRenderThread()
{
	check(IsInGameThread());

	if (!Subsystem || !SceneProxy)
	{
		return false;
	}

	TArray<FOptimizedSkeletalMeshBonePaletteSnapshot> paletteSnapshots;
	Subsystem->GetBonePaletteSnapshots(paletteSnapshots);
	if (paletteSnapshots.IsEmpty())
	{
		return false;
	}

	TArray<OptimizedSkeletalMesh::FBonePaletteRenderSnapshot> renderSnapshots;
	renderSnapshots.Reserve(paletteSnapshots.Num());

	for (FOptimizedSkeletalMeshBonePaletteSnapshot& snapshot : paletteSnapshots)
	{
		if (!snapshot.Handle.IsValid() || snapshot.BonePalette.IsEmpty())
		{
			continue;
		}

		OptimizedSkeletalMesh::FBonePaletteRenderSnapshot& renderSnapshot = renderSnapshots.AddDefaulted_GetRef();
		renderSnapshot.InstanceId = snapshot.Handle.Id;
		renderSnapshot.BonePalette = MoveTemp(snapshot.BonePalette);
	}

	FOptimizedSkeletalMeshSceneProxy* optimizedSceneProxy =
		static_cast<FOptimizedSkeletalMeshSceneProxy*>(SceneProxy);

	ENQUEUE_RENDER_COMMAND(UpdateOptimizedSkeletalMeshBonePalettes)(
		[optimizedSceneProxy, renderSnapshots = MoveTemp(renderSnapshots)](FRHICommandListImmediate& InRHICmdList) mutable {
			optimizedSceneProxy->UpdateBonePalettes_RenderThread(InRHICmdList, MoveTemp(renderSnapshots));
		});

	return true;
}

void UOptimizedSkeletalMeshRenderComponent::ApplyRenderStats_GameThread(
	const FOptimizedSkeletalMeshRenderStats& InStats)
{
	check(IsInGameThread());
	LastRenderStats = InStats;
	if (Subsystem)
	{
		Subsystem->UpdateRenderVisibleInstanceIds(MakeArrayView(InStats.RenderVisibleInstanceIds));
	}

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
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSkinningSkinWeightLODs, InStats.SkinningSkinWeightLODs);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSkinningSkinWeightVertices, InStats.SkinningSkinWeightVertices);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSkinningMaxBoneInfluences, InStats.SkinningMaxBoneInfluences);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSkinning16BitIndexLODs, InStats.SkinningSkinWeight16BitIndexLODs);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSkinning16BitWeightLODs, InStats.SkinningSkinWeight16BitWeightLODs);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSkinningMissingSkinWeightLODs, InStats.SkinningMissingSkinWeightLODs);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSkinningGPUSkinReadyLODs, InStats.SkinningGPUSkinReadyLODs);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSkinningGPUSkinFallbackDraws, InStats.SkinningGPUSkinFallbackDraws);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSkinningGPUSkinDraws, InStats.SkinningGPUSkinDraws);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSkinningGPUInstanceOffsetEntries, InStats.SkinningGPUInstanceOffsetEntries);
	SET_DWORD_STAT(STAT_OptimizedSkeletalMeshSkinningGPUSectionBoneMapEntries, InStats.SkinningGPUSectionBoneMapEntries);
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

FBoxSphereBounds UOptimizedSkeletalMeshRenderComponent::CalcBounds(const FTransform& InLocalToWorld) const
{
	FBox worldBox(ForceInit);

	if (Subsystem)
	{
		TArray<FOptimizedSkeletalMeshInstanceSnapshot> snapshots;
		Subsystem->GetInstancesSnapshot(snapshots);

		for (const FOptimizedSkeletalMeshInstanceSnapshot& snapshot : snapshots)
		{
			if (snapshot.Desc.bVisible)
			{
				worldBox += OptimizedSkeletalMesh::GetInstanceWorldBounds(snapshot.Desc);
			}
		}
	}

	if (!worldBox.IsValid)
	{
		worldBox = FBox::BuildAABB(InLocalToWorld.GetLocation(), FVector(1.0f));
	}

	if (bUseConservativeProxyBounds)
	{
		return FBoxSphereBounds(FBox::BuildAABB(worldBox.GetCenter(), FVector(ConservativeProxyBoundsExtent)));
	}

	return FBoxSphereBounds(worldBox);
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

	TArray<FOptimizedSkeletalMeshInstanceSnapshot> snapshots;
	Subsystem->GetInstancesSnapshot(snapshots);

	for (const FOptimizedSkeletalMeshInstanceSnapshot& snapshot : snapshots)
	{
		if (!snapshot.Desc.bVisible || !snapshot.Desc.SkeletalMesh)
		{
			continue;
		}

		for (const FSkeletalMaterial& skeletalMaterial : snapshot.Desc.SkeletalMesh->GetMaterials())
		{
			if (skeletalMaterial.MaterialInterface)
			{
				OutMaterials.AddUnique(skeletalMaterial.MaterialInterface);
			}
		}
	}

	if (UMaterialInterface* defaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface))
	{
		OutMaterials.AddUnique(defaultMaterial);
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
