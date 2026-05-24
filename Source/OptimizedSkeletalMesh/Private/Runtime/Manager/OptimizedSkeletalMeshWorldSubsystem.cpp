// Copyright Epic Games, Inc. All Rights Reserved.

#include "Runtime/Manager/OptimizedSkeletalMeshWorldSubsystem.h"

#include "Async/ParallelFor.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"
#include "BonePose.h"
#include "Components/SceneComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/ThreadSafeCounter.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Runtime/Rendering/OptimizedSkeletalMeshRenderBridgeActor.h"
#include "Runtime/Rendering/OptimizedSkeletalMeshRenderComponent.h"
#include "Runtime/Settings/OptimizedSkeletalMeshSettings.h"
#include "Stats/Stats.h"
#include "Logging/LogMacros.h"

DECLARE_STATS_GROUP_SORTBYNAME(TEXT("OSM Animation"), STATGROUP_OSMAnimation, STATCAT_Advanced);
DECLARE_DWORD_COUNTER_STAT(TEXT("Registered Instances"), STAT_OptimizedSkeletalMeshAnimationRegisteredInstances, STATGROUP_OSMAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Animated Instances"), STAT_OptimizedSkeletalMeshAnimationAnimatedInstances, STATGROUP_OSMAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Active Animation Instances"), STAT_OptimizedSkeletalMeshAnimationActiveAnimationInstances, STATGROUP_OSMAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Dirty Animation Instances"), STAT_OptimizedSkeletalMeshAnimationDirtyAnimationInstances, STATGROUP_OSMAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Skipped Update Rate Instances"), STAT_OptimizedSkeletalMeshAnimationSkippedUpdateRateInstances, STATGROUP_OSMAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Distance Rate Scaled Instances"), STAT_OptimizedSkeletalMeshAnimationDistanceRateScaledInstances, STATGROUP_OSMAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Parallel Pose Batches"), STAT_OptimizedSkeletalMeshAnimationParallelPoseBatches, STATGROUP_OSMAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Advanced Instances"), STAT_OptimizedSkeletalMeshAnimationAdvancedInstances, STATGROUP_OSMAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Finished Instances"), STAT_OptimizedSkeletalMeshAnimationFinishedInstances, STATGROUP_OSMAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Pose Evaluated Instances"), STAT_OptimizedSkeletalMeshAnimationPoseEvaluatedInstances, STATGROUP_OSMAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Failed Pose Evaluations"), STAT_OptimizedSkeletalMeshAnimationFailedPoseEvaluations, STATGROUP_OSMAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Bone Palette Instances"), STAT_OptimizedSkeletalMeshAnimationBonePaletteInstances, STATGROUP_OSMAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Render Visible Animated Instances"), STAT_OptimizedSkeletalMeshAnimationRenderVisibleAnimatedInstances, STATGROUP_OSMAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Dirty CPU Palette Instances"), STAT_OptimizedSkeletalMeshAnimationDirtyCpuPaletteInstances, STATGROUP_OSMAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Dirty GPU Palette Instances"), STAT_OptimizedSkeletalMeshAnimationDirtyGpuPaletteInstances, STATGROUP_OSMAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("GPU Palette Upload Skipped Instances"), STAT_OptimizedSkeletalMeshAnimationGpuPaletteUploadSkippedInstances, STATGROUP_OSMAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Total Bone Matrices"), STAT_OptimizedSkeletalMeshAnimationTotalBoneMatrices, STATGROUP_OSMAnimation);
DECLARE_DWORD_COUNTER_STAT(TEXT("Max Bones Per Instance"), STAT_OptimizedSkeletalMeshAnimationMaxBonesPerInstance, STATGROUP_OSMAnimation);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Min Effective Update Rate Hz"), STAT_OptimizedSkeletalMeshAnimationMinEffectiveUpdateRateHz, STATGROUP_OSMAnimation);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Max Effective Update Rate Hz"), STAT_OptimizedSkeletalMeshAnimationMaxEffectiveUpdateRateHz, STATGROUP_OSMAnimation);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Average Effective Update Rate Hz"), STAT_OptimizedSkeletalMeshAnimationAverageEffectiveUpdateRateHz, STATGROUP_OSMAnimation);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Last Delta Seconds"), STAT_OptimizedSkeletalMeshAnimationLastDeltaSeconds, STATGROUP_OSMAnimation);

namespace OptimizedSkeletalMesh
{
	static FThreadSafeCounter RenderCVarChangeVersion;
	static bool bRenderSettingsSyncInProgress = false;
	static bool bEnableSettingsBackSync = false;

	static void SetConsoleInt(const TCHAR* InName, const int32 InValue)
	{
		if (IConsoleVariable* cvar = IConsoleManager::Get().FindConsoleVariable(InName))
		{
			cvar->Set(InValue, ECVF_SetByCode);
		}
	}

	static void SetConsoleFloat(const TCHAR* InName, const float InValue)
	{
		if (IConsoleVariable* cvar = IConsoleManager::Get().FindConsoleVariable(InName))
		{
			cvar->Set(InValue, ECVF_SetByCode);
		}
	}

	static void SyncSettingsFromRenderCVars()
	{
		if (bRenderSettingsSyncInProgress || !bEnableSettingsBackSync)
		{
			return;
		}

		UOptimizedSkeletalMeshSettings* settings = GetMutableDefault<UOptimizedSkeletalMeshSettings>();
		if (!settings)
		{
			return;
		}

		TGuardValue<bool> syncGuard(bRenderSettingsSyncInProgress, true);
		auto getInt = [](const TCHAR* InName, const int32 InFallback) {
			if (IConsoleVariable* cvar = IConsoleManager::Get().FindConsoleVariable(InName))
			{
				return cvar->GetInt();
			}

			return InFallback;
		};
		auto getFloat = [](const TCHAR* InName, const float InFallback) {
			if (IConsoleVariable* cvar = IConsoleManager::Get().FindConsoleVariable(InName))
			{
				return cvar->GetFloat();
			}

			return InFallback;
		};

		settings->bDrawDebugBounds = getInt(TEXT("osm.Render.DrawDebugBounds"), 0) != 0;
		settings->bDrawMeshSections = getInt(TEXT("osm.Render.DrawMeshSections"), 1) != 0;
		settings->MeshDrawMode = static_cast<EOptimizedSkeletalMeshDrawMode>(
			FMath::Clamp(getInt(TEXT("osm.Render.DrawMode"), 3), 0, 3));
		settings->bEnableInstanceFrustumCulling = getInt(TEXT("osm.Render.InstanceFrustumCulling"), 1) != 0;
		settings->InstanceCullBoundsScale = FMath::Max(1.0f, getFloat(TEXT("osm.Render.InstanceCullBoundsScale"), 1.5f));
		settings->bUseConservativeProxyBounds = getInt(TEXT("osm.Render.UseConservativeProxyBounds"), 1) != 0;
		settings->ConservativeProxyBoundsExtent =
			FMath::Max(1000.0f, getFloat(TEXT("osm.Render.ConservativeProxyBoundsExtent"), 10000000.0f));
		settings->bDrawCullingDebug = getInt(TEXT("osm.Render.DrawCullingDebug"), 0) != 0;
		settings->bDrawCullTestBounds = getInt(TEXT("osm.Render.DrawCullTestBounds"), 0) != 0;
		settings->bCastShadows = getInt(TEXT("osm.Render.CastShadows"), 1) != 0;
		settings->NearFullShadowDistance = FMath::Max(0.0f, getFloat(TEXT("osm.Render.NearFullShadowDistance"), 1800.0f));
		settings->MidShadowDistance = FMath::Max(0.0f, getFloat(TEXT("osm.Render.MidShadowDistance"), 3200.0f));
		settings->MidShadowUpdateDivisor = FMath::Max(1, getInt(TEXT("osm.Render.MidShadowUpdateDivisor"), 2));
		settings->FarShadowUpdateDivisor = FMath::Max(0, getInt(TEXT("osm.Render.FarShadowUpdateDivisor"), 0));
		settings->MaxShadowCastDistance = FMath::Max(0.0f, getFloat(TEXT("osm.Render.MaxShadowCastDistance"), 5000.0f));
		settings->MaxDynamicShadowCasters = FMath::Max(0, getInt(TEXT("osm.Render.MaxDynamicShadowCasters"), 120));
		settings->NearShadowLodBias = FMath::Max(0, getInt(TEXT("osm.Render.NearShadowLodBias"), 0));
		settings->MidShadowLodBias = FMath::Max(0, getInt(TEXT("osm.Render.MidShadowLodBias"), 1));
		settings->FarShadowLodBias = FMath::Max(0, getInt(TEXT("osm.Render.FarShadowLodBias"), 2));
		settings->MaxShadowSectionsPerLOD = FMath::Max(0, getInt(TEXT("osm.Render.MaxShadowSectionsPerLOD"), 2));
		settings->LocalLightMaxShadowCastDistance =
			FMath::Max(0.0f, getFloat(TEXT("osm.Render.LocalLightMaxShadowCastDistance"), 2000.0f));
		settings->LocalLightMaxDynamicShadowCasters =
			FMath::Max(0, getInt(TEXT("osm.Render.LocalLightMaxDynamicShadowCasters"), 24));
		settings->LocalLightShadowLodBias =
			FMath::Max(0, getInt(TEXT("osm.Render.LocalLightShadowLodBias"), 3));
		settings->LocalLightMaxShadowSectionsPerLOD =
			FMath::Max(0, getInt(TEXT("osm.Render.LocalLightMaxShadowSectionsPerLOD"), 1));
#if WITH_EDITOR
		settings->SaveConfig();
#endif
	}

	static void PushRenderSettingsToCVars(const FOptimizedSkeletalMeshRenderSettings& InSettings)
	{
		if (bRenderSettingsSyncInProgress)
		{
			return;
		}

		TGuardValue<bool> syncGuard(bRenderSettingsSyncInProgress, true);
		const int32 drawMode = FMath::Clamp(static_cast<int32>(InSettings.MeshDrawMode), 0, 3);
		SetConsoleInt(TEXT("osm.Render.DrawDebugBounds"), InSettings.bDrawDebugBounds ? 1 : 0);
		SetConsoleInt(TEXT("osm.Render.DrawMeshSections"), InSettings.bDrawMeshSections ? 1 : 0);
		SetConsoleInt(TEXT("osm.Render.DrawMode"), drawMode);
		SetConsoleInt(
			TEXT("osm.Render.InstanceFrustumCulling"),
			InSettings.bEnableInstanceFrustumCulling ? 1 : 0);
		SetConsoleFloat(TEXT("osm.Render.InstanceCullBoundsScale"), FMath::Max(1.0f, InSettings.InstanceCullBoundsScale));
		SetConsoleInt(
			TEXT("osm.Render.UseConservativeProxyBounds"),
			InSettings.bUseConservativeProxyBounds ? 1 : 0);
		SetConsoleFloat(
			TEXT("osm.Render.ConservativeProxyBoundsExtent"),
			FMath::Max(1000.0f, InSettings.ConservativeProxyBoundsExtent));
		SetConsoleInt(TEXT("osm.Render.DrawCullingDebug"), InSettings.bDrawCullingDebug ? 1 : 0);
		SetConsoleInt(TEXT("osm.Render.DrawCullTestBounds"), InSettings.bDrawCullTestBounds ? 1 : 0);
		SetConsoleInt(TEXT("osm.Render.CastShadows"), InSettings.bCastShadows ? 1 : 0);
		SetConsoleFloat(TEXT("osm.Render.NearFullShadowDistance"), FMath::Max(0.0f, InSettings.NearFullShadowDistance));
		SetConsoleFloat(TEXT("osm.Render.MidShadowDistance"), FMath::Max(0.0f, InSettings.MidShadowDistance));
		SetConsoleInt(TEXT("osm.Render.MidShadowUpdateDivisor"), FMath::Max(1, InSettings.MidShadowUpdateDivisor));
		SetConsoleInt(TEXT("osm.Render.FarShadowUpdateDivisor"), FMath::Max(0, InSettings.FarShadowUpdateDivisor));
		SetConsoleFloat(TEXT("osm.Render.MaxShadowCastDistance"), FMath::Max(0.0f, InSettings.MaxShadowCastDistance));
		SetConsoleInt(TEXT("osm.Render.MaxDynamicShadowCasters"), FMath::Max(0, InSettings.MaxDynamicShadowCasters));
		SetConsoleInt(TEXT("osm.Render.NearShadowLodBias"), FMath::Max(0, InSettings.NearShadowLodBias));
		SetConsoleInt(TEXT("osm.Render.MidShadowLodBias"), FMath::Max(0, InSettings.MidShadowLodBias));
		SetConsoleInt(TEXT("osm.Render.FarShadowLodBias"), FMath::Max(0, InSettings.FarShadowLodBias));
		SetConsoleInt(TEXT("osm.Render.MaxShadowSectionsPerLOD"), FMath::Max(0, InSettings.MaxShadowSectionsPerLOD));
		SetConsoleFloat(
			TEXT("osm.Render.LocalLightMaxShadowCastDistance"),
			FMath::Max(0.0f, InSettings.LocalLightMaxShadowCastDistance));
		SetConsoleInt(
			TEXT("osm.Render.LocalLightMaxDynamicShadowCasters"),
			FMath::Max(0, InSettings.LocalLightMaxDynamicShadowCasters));
		SetConsoleInt(
			TEXT("osm.Render.LocalLightShadowLodBias"),
			FMath::Max(0, InSettings.LocalLightShadowLodBias));
		SetConsoleInt(
			TEXT("osm.Render.LocalLightMaxShadowSectionsPerLOD"),
			FMath::Max(0, InSettings.LocalLightMaxShadowSectionsPerLOD));
	}

	static FOptimizedSkeletalMeshRenderSettings BuildRenderSettingsFromProjectSettings()
	{
		FOptimizedSkeletalMeshRenderSettings settings;
		if (const UOptimizedSkeletalMeshSettings* projectSettings = GetDefault<UOptimizedSkeletalMeshSettings>())
		{
			settings.bDrawDebugBounds = projectSettings->bDrawDebugBounds;
			settings.bDrawMeshSections = projectSettings->bDrawMeshSections;
			settings.MeshDrawMode = projectSettings->MeshDrawMode;
			settings.bEnableInstanceFrustumCulling = projectSettings->bEnableInstanceFrustumCulling;
			settings.InstanceCullBoundsScale = projectSettings->InstanceCullBoundsScale;
			settings.bUseConservativeProxyBounds = projectSettings->bUseConservativeProxyBounds;
			settings.ConservativeProxyBoundsExtent = projectSettings->ConservativeProxyBoundsExtent;
			settings.bDrawCullingDebug = projectSettings->bDrawCullingDebug;
			settings.bDrawCullTestBounds = projectSettings->bDrawCullTestBounds;
			settings.bCastShadows = projectSettings->bCastShadows;
			settings.NearFullShadowDistance = projectSettings->NearFullShadowDistance;
			settings.MidShadowDistance = projectSettings->MidShadowDistance;
			settings.MidShadowUpdateDivisor = projectSettings->MidShadowUpdateDivisor;
			settings.FarShadowUpdateDivisor = projectSettings->FarShadowUpdateDivisor;
			settings.MaxShadowCastDistance = projectSettings->MaxShadowCastDistance;
			settings.MaxDynamicShadowCasters = projectSettings->MaxDynamicShadowCasters;
			settings.NearShadowLodBias = projectSettings->NearShadowLodBias;
			settings.MidShadowLodBias = projectSettings->MidShadowLodBias;
			settings.FarShadowLodBias = projectSettings->FarShadowLodBias;
			settings.MaxShadowSectionsPerLOD = projectSettings->MaxShadowSectionsPerLOD;
			settings.LocalLightMaxShadowCastDistance = projectSettings->LocalLightMaxShadowCastDistance;
			settings.LocalLightMaxDynamicShadowCasters = projectSettings->LocalLightMaxDynamicShadowCasters;
			settings.LocalLightShadowLodBias = projectSettings->LocalLightShadowLodBias;
			settings.LocalLightMaxShadowSectionsPerLOD = projectSettings->LocalLightMaxShadowSectionsPerLOD;
		}

		settings.InstanceCullBoundsScale = FMath::Max(1.0f, settings.InstanceCullBoundsScale);
		settings.ConservativeProxyBoundsExtent = FMath::Max(1000.0f, settings.ConservativeProxyBoundsExtent);
		settings.MaxShadowCastDistance = FMath::Max(0.0f, settings.MaxShadowCastDistance);
		settings.NearFullShadowDistance = FMath::Max(0.0f, settings.NearFullShadowDistance);
		settings.MidShadowDistance = FMath::Max(settings.NearFullShadowDistance, settings.MidShadowDistance);
		settings.MidShadowUpdateDivisor = FMath::Max(1, settings.MidShadowUpdateDivisor);
		settings.FarShadowUpdateDivisor = FMath::Max(0, settings.FarShadowUpdateDivisor);
		settings.MaxDynamicShadowCasters = FMath::Max(0, settings.MaxDynamicShadowCasters);
		settings.NearShadowLodBias = FMath::Max(0, settings.NearShadowLodBias);
		settings.MidShadowLodBias = FMath::Max(0, settings.MidShadowLodBias);
		settings.FarShadowLodBias = FMath::Max(0, settings.FarShadowLodBias);
		settings.MaxShadowSectionsPerLOD = FMath::Max(0, settings.MaxShadowSectionsPerLOD);
		settings.LocalLightMaxShadowCastDistance = FMath::Max(0.0f, settings.LocalLightMaxShadowCastDistance);
		settings.LocalLightMaxDynamicShadowCasters = FMath::Max(0, settings.LocalLightMaxDynamicShadowCasters);
		settings.LocalLightShadowLodBias = FMath::Max(0, settings.LocalLightShadowLodBias);
		settings.LocalLightMaxShadowSectionsPerLOD = FMath::Max(0, settings.LocalLightMaxShadowSectionsPerLOD);
		settings.MeshDrawMode = static_cast<EOptimizedSkeletalMeshDrawMode>(
			FMath::Clamp(static_cast<int32>(settings.MeshDrawMode), 0, 3));
		return settings;
	}

	static void OnRenderCVarsChanged()
	{
		SyncSettingsFromRenderCVars();
		RenderCVarChangeVersion.Increment();
	}

	static void SetSettingsBackSyncEnabled(const bool bInEnabled)
	{
		bEnableSettingsBackSync = bInEnabled;
	}

	static FAutoConsoleVariableSink RenderCVarSink(
		FConsoleCommandDelegate::CreateStatic(&OnRenderCVarsChanged));

	static int32 GetRenderCVarChangeVersion()
	{
		return RenderCVarChangeVersion.GetValue();
	}

	static TAutoConsoleVariable<int32> CVarRenderDrawDebugBounds(
		TEXT("osm.Render.DrawDebugBounds"),
		0,
		TEXT("bDrawDebugBounds.\n0: false, 1: true"),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRenderDrawMeshSections(
		TEXT("osm.Render.DrawMeshSections"),
		1,
		TEXT("bDrawMeshSections.\n0: false, 1: true"),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRenderDrawMode(
		TEXT("osm.Render.DrawMode"),
		static_cast<int32>(EOptimizedSkeletalMeshDrawMode::GpuSkinnedInstanced),
		TEXT("MeshDrawMode.\n")
		TEXT("0: DynamicMeshProof, 1: DirectMeshPerInstance, 2: DirectMeshInstanced, 3: GpuSkinnedInstanced"),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRenderInstanceFrustumCulling(
		TEXT("osm.Render.InstanceFrustumCulling"),
		1,
		TEXT("bEnableInstanceFrustumCulling.\n0: false, 1: true"),
		ECVF_Default);

	static TAutoConsoleVariable<float> CVarRenderInstanceCullBoundsScale(
		TEXT("osm.Render.InstanceCullBoundsScale"),
		1.5f,
		TEXT("InstanceCullBoundsScale."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRenderUseConservativeProxyBounds(
		TEXT("osm.Render.UseConservativeProxyBounds"),
		1,
		TEXT("bUseConservativeProxyBounds.\n0: false, 1: true"),
		ECVF_Default);

	static TAutoConsoleVariable<float> CVarRenderConservativeProxyBoundsExtent(
		TEXT("osm.Render.ConservativeProxyBoundsExtent"),
		10000000.0f,
		TEXT("ConservativeProxyBoundsExtent."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRenderDrawCullingDebug(
		TEXT("osm.Render.DrawCullingDebug"),
		0,
		TEXT("bDrawCullingDebug.\n0: false, 1: true"),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRenderDrawCullTestBounds(
		TEXT("osm.Render.DrawCullTestBounds"),
		0,
		TEXT("bDrawCullTestBounds.\n0: false, 1: true"),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRenderCastShadows(
		TEXT("osm.Render.CastShadows"),
		1,
		TEXT("bCastShadows.\n0: false, 1: true"),
		ECVF_Default);

	static TAutoConsoleVariable<float> CVarRenderNearFullShadowDistance(
		TEXT("osm.Render.NearFullShadowDistance"),
		1800.0f,
		TEXT("NearFullShadowDistance.\n0 means disabled."),
		ECVF_Default);

	static TAutoConsoleVariable<float> CVarRenderMidShadowDistance(
		TEXT("osm.Render.MidShadowDistance"),
		3200.0f,
		TEXT("MidShadowDistance."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRenderMidShadowUpdateDivisor(
		TEXT("osm.Render.MidShadowUpdateDivisor"),
		2,
		TEXT("MidShadowUpdateDivisor.\n1 means every frame."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRenderFarShadowUpdateDivisor(
		TEXT("osm.Render.FarShadowUpdateDivisor"),
		0,
		TEXT("FarShadowUpdateDivisor.\n0 means disabled."),
		ECVF_Default);

	static TAutoConsoleVariable<float> CVarRenderMaxShadowCastDistance(
		TEXT("osm.Render.MaxShadowCastDistance"),
		5000.0f,
		TEXT("MaxShadowCastDistance.\n<= 0 means no distance limit."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRenderMaxDynamicShadowCasters(
		TEXT("osm.Render.MaxDynamicShadowCasters"),
		120,
		TEXT("MaxDynamicShadowCasters.\n<= 0 means unlimited."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRenderNearShadowLodBias(
		TEXT("osm.Render.NearShadowLodBias"),
		0,
		TEXT("NearShadowLodBias."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRenderMidShadowLodBias(
		TEXT("osm.Render.MidShadowLodBias"),
		1,
		TEXT("MidShadowLodBias."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRenderFarShadowLodBias(
		TEXT("osm.Render.FarShadowLodBias"),
		2,
		TEXT("FarShadowLodBias."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRenderMaxShadowSectionsPerLOD(
		TEXT("osm.Render.MaxShadowSectionsPerLOD"),
		2,
		TEXT("MaxShadowSectionsPerLOD.\n0 means unlimited."),
		ECVF_Default);

	static TAutoConsoleVariable<float> CVarRenderLocalLightMaxShadowCastDistance(
		TEXT("osm.Render.LocalLightMaxShadowCastDistance"),
		2000.0f,
		TEXT("LocalLightMaxShadowCastDistance.\n<= 0 means no limit."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRenderLocalLightMaxDynamicShadowCasters(
		TEXT("osm.Render.LocalLightMaxDynamicShadowCasters"),
		24,
		TEXT("LocalLightMaxDynamicShadowCasters.\n<= 0 means unlimited."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRenderLocalLightShadowLodBias(
		TEXT("osm.Render.LocalLightShadowLodBias"),
		3,
		TEXT("LocalLightShadowLodBias."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRenderLocalLightMaxShadowSectionsPerLOD(
		TEXT("osm.Render.LocalLightMaxShadowSectionsPerLOD"),
		1,
		TEXT("LocalLightMaxShadowSectionsPerLOD.\n0 means unlimited."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarAnimationMaxDirtyEvaluationsPerFrame(
		TEXT("osm.Animation.MaxDirtyEvaluationsPerFrame"),
		512,
		TEXT("Maximum number of dirty animation instances to evaluate per frame.\n")
		TEXT("<= 0 means no limit."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarAnimationPauseCpuPoseWhenNotVisible(
		TEXT("osm.Animation.PauseCpuPoseWhenNotVisible"),
		1,
		TEXT("Skip CPU pose eval for non-render-visible instances and only advance animation time.\n")
		TEXT("0: disabled, 1: enabled."),
		ECVF_Default);

	static TAutoConsoleVariable<float> CVarAnimationInvisiblePoseTickRateHz(
		TEXT("osm.Animation.InvisiblePoseTickRateHz"),
		0.0f,
		TEXT("Optional tick rate used while instance is non-render-visible.\n")
		TEXT("<= 0 means advance time every frame without CPU pose eval."),
		ECVF_Default);

	static bool bDebugEntitiesEnabled = false;
	static float DebugEntitiesRadius = 5000.0f;
	static int32 DebugEntitiesMaxCount = 64;

	static FAutoConsoleCommand DebugEntitiesShowCommand(
		TEXT("osm.DebugEntities.Show"),
		TEXT("Show per-instance OSM debug text in world near camera."),
		FConsoleCommandDelegate::CreateLambda([]() {
			bDebugEntitiesEnabled = true;
		}));

	static FAutoConsoleCommand DebugEntitiesHideCommand(
		TEXT("osm.DebugEntities.Hide"),
		TEXT("Hide per-instance OSM debug text in world."),
		FConsoleCommandDelegate::CreateLambda([]() {
			bDebugEntitiesEnabled = false;
		}));

	static FAutoConsoleCommand DebugEntitiesToggleCommand(
		TEXT("osm.DebugEntities.Toggle"),
		TEXT("Toggle per-instance OSM debug text in world."),
		FConsoleCommandDelegate::CreateLambda([]() {
			bDebugEntitiesEnabled = !bDebugEntitiesEnabled;
		}));

	static FOptimizedSkeletalMeshRenderSettings NormalizeRenderSettings(const FOptimizedSkeletalMeshRenderSettings& InSettings)
	{
		FOptimizedSkeletalMeshRenderSettings normalizedSettings = InSettings;
		normalizedSettings.InstanceCullBoundsScale = FMath::Max(1.0f, InSettings.InstanceCullBoundsScale);
		normalizedSettings.ConservativeProxyBoundsExtent = FMath::Max(1000.0f, InSettings.ConservativeProxyBoundsExtent);
		normalizedSettings.NearFullShadowDistance = FMath::Max(0.0f, InSettings.NearFullShadowDistance);
		normalizedSettings.MidShadowDistance = FMath::Max(normalizedSettings.NearFullShadowDistance, InSettings.MidShadowDistance);
		normalizedSettings.MidShadowUpdateDivisor = FMath::Max(1, InSettings.MidShadowUpdateDivisor);
		normalizedSettings.FarShadowUpdateDivisor = FMath::Max(0, InSettings.FarShadowUpdateDivisor);
		normalizedSettings.MaxShadowCastDistance = FMath::Max(0.0f, InSettings.MaxShadowCastDistance);
		normalizedSettings.MaxDynamicShadowCasters = FMath::Max(0, InSettings.MaxDynamicShadowCasters);
		normalizedSettings.NearShadowLodBias = FMath::Max(0, InSettings.NearShadowLodBias);
		normalizedSettings.MidShadowLodBias = FMath::Max(0, InSettings.MidShadowLodBias);
		normalizedSettings.FarShadowLodBias = FMath::Max(0, InSettings.FarShadowLodBias);
		normalizedSettings.MaxShadowSectionsPerLOD = FMath::Max(0, InSettings.MaxShadowSectionsPerLOD);
		normalizedSettings.LocalLightMaxShadowCastDistance = FMath::Max(0.0f, InSettings.LocalLightMaxShadowCastDistance);
		normalizedSettings.LocalLightMaxDynamicShadowCasters = FMath::Max(0, InSettings.LocalLightMaxDynamicShadowCasters);
		normalizedSettings.LocalLightShadowLodBias = FMath::Max(0, InSettings.LocalLightShadowLodBias);
		normalizedSettings.LocalLightMaxShadowSectionsPerLOD = FMath::Max(0, InSettings.LocalLightMaxShadowSectionsPerLOD);
		normalizedSettings.MeshDrawMode = static_cast<EOptimizedSkeletalMeshDrawMode>(
			FMath::Clamp(static_cast<int32>(InSettings.MeshDrawMode), 0, 3));
		return normalizedSettings;
	}

	static FOptimizedSkeletalMeshRenderSettings ResolveRenderSettingsFromCVars()
	{
		FOptimizedSkeletalMeshRenderSettings resolvedSettings;
		resolvedSettings.bDrawDebugBounds = CVarRenderDrawDebugBounds.GetValueOnGameThread() != 0;
		resolvedSettings.bDrawMeshSections = CVarRenderDrawMeshSections.GetValueOnGameThread() != 0;
		resolvedSettings.MeshDrawMode = static_cast<EOptimizedSkeletalMeshDrawMode>(
			FMath::Clamp(CVarRenderDrawMode.GetValueOnGameThread(), 0, 3));
		resolvedSettings.bEnableInstanceFrustumCulling = CVarRenderInstanceFrustumCulling.GetValueOnGameThread() != 0;
		resolvedSettings.InstanceCullBoundsScale = CVarRenderInstanceCullBoundsScale.GetValueOnGameThread();
		resolvedSettings.bUseConservativeProxyBounds = CVarRenderUseConservativeProxyBounds.GetValueOnGameThread() != 0;
		resolvedSettings.ConservativeProxyBoundsExtent = CVarRenderConservativeProxyBoundsExtent.GetValueOnGameThread();
		resolvedSettings.bDrawCullingDebug = CVarRenderDrawCullingDebug.GetValueOnGameThread() != 0;
		resolvedSettings.bDrawCullTestBounds = CVarRenderDrawCullTestBounds.GetValueOnGameThread() != 0;
		resolvedSettings.bCastShadows = CVarRenderCastShadows.GetValueOnGameThread() != 0;
		resolvedSettings.NearFullShadowDistance = CVarRenderNearFullShadowDistance.GetValueOnGameThread();
		resolvedSettings.MidShadowDistance = CVarRenderMidShadowDistance.GetValueOnGameThread();
		resolvedSettings.MidShadowUpdateDivisor = CVarRenderMidShadowUpdateDivisor.GetValueOnGameThread();
		resolvedSettings.FarShadowUpdateDivisor = CVarRenderFarShadowUpdateDivisor.GetValueOnGameThread();
		resolvedSettings.MaxShadowCastDistance = CVarRenderMaxShadowCastDistance.GetValueOnGameThread();
		resolvedSettings.MaxDynamicShadowCasters = CVarRenderMaxDynamicShadowCasters.GetValueOnGameThread();
		resolvedSettings.NearShadowLodBias = CVarRenderNearShadowLodBias.GetValueOnGameThread();
		resolvedSettings.MidShadowLodBias = CVarRenderMidShadowLodBias.GetValueOnGameThread();
		resolvedSettings.FarShadowLodBias = CVarRenderFarShadowLodBias.GetValueOnGameThread();
		resolvedSettings.MaxShadowSectionsPerLOD = CVarRenderMaxShadowSectionsPerLOD.GetValueOnGameThread();
		resolvedSettings.LocalLightMaxShadowCastDistance = CVarRenderLocalLightMaxShadowCastDistance.GetValueOnGameThread();
		resolvedSettings.LocalLightMaxDynamicShadowCasters = CVarRenderLocalLightMaxDynamicShadowCasters.GetValueOnGameThread();
		resolvedSettings.LocalLightShadowLodBias = CVarRenderLocalLightShadowLodBias.GetValueOnGameThread();
		resolvedSettings.LocalLightMaxShadowSectionsPerLOD = CVarRenderLocalLightMaxShadowSectionsPerLOD.GetValueOnGameThread();

		return NormalizeRenderSettings(resolvedSettings);
	}

	static bool AreRenderSettingsEqual(
		const FOptimizedSkeletalMeshRenderSettings& InLeft,
		const FOptimizedSkeletalMeshRenderSettings& InRight)
	{
		return InLeft.bDrawDebugBounds == InRight.bDrawDebugBounds
			&& InLeft.bDrawMeshSections == InRight.bDrawMeshSections
			&& InLeft.MeshDrawMode == InRight.MeshDrawMode
			&& InLeft.bEnableInstanceFrustumCulling == InRight.bEnableInstanceFrustumCulling
			&& FMath::IsNearlyEqual(InLeft.InstanceCullBoundsScale, InRight.InstanceCullBoundsScale)
			&& InLeft.bUseConservativeProxyBounds == InRight.bUseConservativeProxyBounds
			&& FMath::IsNearlyEqual(InLeft.ConservativeProxyBoundsExtent, InRight.ConservativeProxyBoundsExtent)
			&& InLeft.bDrawCullingDebug == InRight.bDrawCullingDebug
			&& InLeft.bDrawCullTestBounds == InRight.bDrawCullTestBounds
			&& InLeft.bCastShadows == InRight.bCastShadows
			&& FMath::IsNearlyEqual(InLeft.NearFullShadowDistance, InRight.NearFullShadowDistance)
			&& FMath::IsNearlyEqual(InLeft.MidShadowDistance, InRight.MidShadowDistance)
			&& InLeft.MidShadowUpdateDivisor == InRight.MidShadowUpdateDivisor
			&& InLeft.FarShadowUpdateDivisor == InRight.FarShadowUpdateDivisor
			&& FMath::IsNearlyEqual(InLeft.MaxShadowCastDistance, InRight.MaxShadowCastDistance)
			&& InLeft.MaxDynamicShadowCasters == InRight.MaxDynamicShadowCasters
			&& InLeft.NearShadowLodBias == InRight.NearShadowLodBias
			&& InLeft.MidShadowLodBias == InRight.MidShadowLodBias
			&& InLeft.FarShadowLodBias == InRight.FarShadowLodBias
			&& InLeft.MaxShadowSectionsPerLOD == InRight.MaxShadowSectionsPerLOD
			&& FMath::IsNearlyEqual(InLeft.LocalLightMaxShadowCastDistance, InRight.LocalLightMaxShadowCastDistance)
			&& InLeft.LocalLightMaxDynamicShadowCasters == InRight.LocalLightMaxDynamicShadowCasters
			&& InLeft.LocalLightShadowLodBias == InRight.LocalLightShadowLodBias
			&& InLeft.LocalLightMaxShadowSectionsPerLOD == InRight.LocalLightMaxShadowSectionsPerLOD;
	}

	static void PublishAnimationStats(const FOptimizedSkeletalMeshAnimationStats& InStats)
	{
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationRegisteredInstances, InStats.RegisteredInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationAnimatedInstances, InStats.AnimatedInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationActiveAnimationInstances, InStats.ActiveAnimationInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationDirtyAnimationInstances, InStats.DirtyAnimationInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationSkippedUpdateRateInstances, InStats.SkippedUpdateRateInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationDistanceRateScaledInstances, InStats.DistanceRateScaledInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationParallelPoseBatches, InStats.ParallelPoseBatches);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationAdvancedInstances, InStats.AdvancedInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationFinishedInstances, InStats.FinishedInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationPoseEvaluatedInstances, InStats.PoseEvaluatedInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationFailedPoseEvaluations, InStats.FailedPoseEvaluations);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationBonePaletteInstances, InStats.BonePaletteInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationRenderVisibleAnimatedInstances, InStats.RenderVisibleAnimatedInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationDirtyCpuPaletteInstances, InStats.DirtyCpuPaletteInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationDirtyGpuPaletteInstances, InStats.DirtyGpuPaletteInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationGpuPaletteUploadSkippedInstances, InStats.GpuPaletteUploadSkippedInstances);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationTotalBoneMatrices, InStats.TotalBoneMatrices);
		SET_DWORD_STAT(STAT_OptimizedSkeletalMeshAnimationMaxBonesPerInstance, InStats.MaxBonesPerInstance);
		SET_FLOAT_STAT(STAT_OptimizedSkeletalMeshAnimationMinEffectiveUpdateRateHz, InStats.MinEffectiveUpdateRateHz);
		SET_FLOAT_STAT(STAT_OptimizedSkeletalMeshAnimationMaxEffectiveUpdateRateHz, InStats.MaxEffectiveUpdateRateHz);
		SET_FLOAT_STAT(STAT_OptimizedSkeletalMeshAnimationAverageEffectiveUpdateRateHz, InStats.AverageEffectiveUpdateRateHz);
		SET_FLOAT_STAT(STAT_OptimizedSkeletalMeshAnimationLastDeltaSeconds, InStats.LastDeltaTime);
	}

	struct FTickAnimationPolicyContext
	{
		bool bPauseCpuPoseWhenNotVisible = false;
		float InvisiblePoseTickRateHz = 0.0f;
		bool bUsesDistanceRateScaling = false;
	};

	static bool ShouldUseInvisiblePosePath(
		const FTickAnimationPolicyContext& InPolicyContext,
		const bool bInRenderVisible)
	{
		return InPolicyContext.bPauseCpuPoseWhenNotVisible && !bInRenderVisible;
	}

	static bool ShouldCollectDistanceRateStats(
		const FTickAnimationPolicyContext& InPolicyContext,
		const bool bInHasCameraDistance,
		const float InEffectiveUpdateRateHz)
	{
		return InPolicyContext.bUsesDistanceRateScaling && bInHasCameraDistance && InEffectiveUpdateRateHz > 0.0f;
	}

} // namespace OptimizedSkeletalMesh

void UOptimizedSkeletalMeshWorldSubsystem::Initialize(FSubsystemCollectionBase& InCollection)
{
	Super::Initialize(InCollection);
	OptimizedSkeletalMesh::SetSettingsBackSyncEnabled(true);

	Instances.Reset();
	FreeInstanceIds.Reset();
	AnimationMeshCaches.Reset();
	PreviousInstanceBonePalettes.Reset();
	InstanceBonePalettes.Reset();
	InstanceAnimationBlendAlphas.Reset();
	ActiveAnimationInstanceIds.Reset();
	DirtyAnimationInstanceIds.Reset();
	DirtyBonePaletteInstanceIds.Reset();
	DirtyTransformInstanceIds.Reset();
	RenderVisibleInstanceIds.Reset();
	AnimationUpdateAccumulators.Reset();
	MaterialTextureOverrideCache.Reset();
	NamedMaterialParamSlots.Reset();
	NextNamedMaterialParamSlot = 0;
	ExternalRenderComponents.Reset();
	CustomDepthRenderComponents.Reset();
	NextInstanceId = 1;
	LastSeenRenderCVarVersion = OptimizedSkeletalMesh::GetRenderCVarChangeVersion();
	RenderStateRecoveryAttempts = 0;
	bRenderDataDirty = false;
	bCustomDepthRenderDataDirty = false;
	LastAnimationStats = FOptimizedSkeletalMeshAnimationStats();
	LastRenderStats = FOptimizedSkeletalMeshRenderStats();
	CurrentRenderSettings = OptimizedSkeletalMesh::BuildRenderSettingsFromProjectSettings();
	OptimizedSkeletalMesh::PushRenderSettingsToCVars(CurrentRenderSettings);
	ActiveRenderSettings = CurrentRenderSettings;
	OptimizedSkeletalMesh::PublishAnimationStats(LastAnimationStats);
	RefreshActiveRenderSettings(true);
	EnsureRenderBridge();
}

void UOptimizedSkeletalMeshWorldSubsystem::Deinitialize()
{
	OptimizedSkeletalMesh::SetSettingsBackSyncEnabled(false);
	DestroyRenderBridge();

	Instances.Reset();
	FreeInstanceIds.Reset();
	AnimationMeshCaches.Reset();
	PreviousInstanceBonePalettes.Reset();
	InstanceBonePalettes.Reset();
	InstanceAnimationBlendAlphas.Reset();
	ActiveAnimationInstanceIds.Reset();
	DirtyAnimationInstanceIds.Reset();
	DirtyBonePaletteInstanceIds.Reset();
	DirtyTransformInstanceIds.Reset();
	RenderVisibleInstanceIds.Reset();
	AnimationUpdateAccumulators.Reset();
	MaterialTextureOverrideCache.Reset();
	NamedMaterialParamSlots.Reset();
	NextNamedMaterialParamSlot = 0;
	ExternalRenderComponents.Reset();
	CustomDepthRenderComponents.Reset();
	NextInstanceId = 1;
	LastSeenRenderCVarVersion = OptimizedSkeletalMesh::GetRenderCVarChangeVersion();
	RenderStateRecoveryAttempts = 0;
	bRenderDataDirty = false;
	bCustomDepthRenderDataDirty = false;
	LastAnimationStats = FOptimizedSkeletalMeshAnimationStats();
	LastRenderStats = FOptimizedSkeletalMeshRenderStats();
	CurrentRenderSettings = OptimizedSkeletalMesh::BuildRenderSettingsFromProjectSettings();
	ActiveRenderSettings = CurrentRenderSettings;
	OptimizedSkeletalMesh::PublishAnimationStats(LastAnimationStats);

	Super::Deinitialize();
}

void UOptimizedSkeletalMeshWorldSubsystem::Tick(float InDeltaTime)
{
	const int32 currentCVarVersion = OptimizedSkeletalMesh::GetRenderCVarChangeVersion();
	if (LastSeenRenderCVarVersion != currentCVarVersion)
	{
		LastSeenRenderCVarVersion = currentCVarVersion;
		RefreshActiveRenderSettings(true);
	}
	else
	{
		RefreshActiveRenderSettings(false);
	}

	EnsureRenderBridge();
	if (RenderComponent && RenderComponent->IsRegistered() && !RenderComponent->IsRenderStateCreated())
	{
		UWorld* world = GetWorld();
		if (world && world->IsGameWorld() && world->GetNetMode() != NM_DedicatedServer && RenderStateRecoveryAttempts < 8)
		{
			++RenderStateRecoveryAttempts;
			RenderComponent->ReregisterComponent();
			RenderComponent->MarkRenderStateDirty();
			RenderComponent->RequestRenderRefresh();
		}
	}
	else if (RenderComponent && RenderComponent->IsRenderStateCreated())
	{
		RenderStateRecoveryAttempts = 0;
	}

	TickAnimation(InDeltaTime);
	if (bRenderDataDirty || bCustomDepthRenderDataDirty)
	{
		RefreshCustomDepthRenderComponents();
	}

	if (HasDirtyRenderVisibleBonePalettes() && PushBonePalettesToRenderComponents())
	{
		ClearDirtyRenderVisibleBonePalettes();
	}

	if (bRenderDataDirty)
	{
		RequestRenderRefreshForAllComponents();

		ClearRenderDataDirty();
		ClearDirtyTransforms();
	}
	else if (bCustomDepthRenderDataDirty)
	{
		RequestCustomDepthRenderRefresh();
		bCustomDepthRenderDataDirty = false;
	}

	if (!bRenderDataDirty && HasDirtyTransforms())
	{
		if (PushInstanceTransformsToRenderComponents())
		{
			ClearDirtyTransforms();
		}
	}

	DrawInstanceDebugOverlay();
}

TStatId UOptimizedSkeletalMeshWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UOptimizedSkeletalMeshWorldSubsystem, STATGROUP_Tickables);
}

bool UOptimizedSkeletalMeshWorldSubsystem::IsTickable() const
{
	const bool bHasPendingRenderCVarUpdate =
		LastSeenRenderCVarVersion != OptimizedSkeletalMesh::GetRenderCVarChangeVersion();
	return !IsTemplate()
		&& (bHasPendingRenderCVarUpdate
			|| bRenderDataDirty
			|| bCustomDepthRenderDataDirty
			|| HasDirtyTransforms()
			|| HasDirtyRenderVisibleBonePalettes()
			|| !ActiveAnimationInstanceIds.IsEmpty()
			|| !DirtyAnimationInstanceIds.IsEmpty());
}

FOptimizedSkeletalMeshInstanceHandle UOptimizedSkeletalMeshWorldSubsystem::RegisterInstance(
	const FOptimizedSkeletalMeshInstanceDesc& InDesc)
{
	if (!InDesc.SkeletalMesh)
	{
		return FOptimizedSkeletalMeshInstanceHandle();
	}

	EnsureRenderBridge();

	const int32 instanceId = AllocateInstanceId();
	Instances.Add(instanceId, InDesc);
	RefreshAnimationTracking(instanceId, InDesc, true);
	MarkRenderDataDirty();

	return FOptimizedSkeletalMeshInstanceHandle(instanceId);
}

bool UOptimizedSkeletalMeshWorldSubsystem::UnregisterInstance(FOptimizedSkeletalMeshInstanceHandle InHandle)
{
	if (!IsValidInstanceId(InHandle.Id))
	{
		return false;
	}

	Instances.Remove(InHandle.Id);
	PreviousInstanceBonePalettes.Remove(InHandle.Id);
	InstanceBonePalettes.Remove(InHandle.Id);
	InstanceAnimationBlendAlphas.Remove(InHandle.Id);
	RemoveAnimationTracking(InHandle.Id);
	MarkBonePaletteDirty(InHandle.Id);
	FreeInstanceIds.Add(InHandle.Id);
	MarkRenderDataDirty();

	return true;
}

FOptimizedSkeletalMeshInstanceHandle UOptimizedSkeletalMeshWorldSubsystem::AddInstance(
	const FOptimizedSkeletalMeshInstanceDesc& InDesc)
{
	return RegisterInstance(InDesc);
}

void UOptimizedSkeletalMeshWorldSubsystem::AddInstancesBatch(
	const FOptimizedSkeletalMeshInstanceDesc& InBaseDesc,
	const TArray<FTransform>& InWorldTransforms,
	TArray<FOptimizedSkeletalMeshInstanceHandle>& OutHandles)
{
	OutHandles.Reset();
	OutHandles.Reserve(InWorldTransforms.Num());

	if (!InBaseDesc.SkeletalMesh)
	{
		return;
	}

	++BulkUpdateDepth;
	for (const FTransform& worldTransform : InWorldTransforms)
	{
		FOptimizedSkeletalMeshInstanceDesc desc = InBaseDesc;
		desc.WorldTransform = worldTransform;

		const FOptimizedSkeletalMeshInstanceHandle handle = RegisterInstance(desc);
		if (handle.IsValid())
		{
			OutHandles.Add(handle);
		}
	}
	BulkUpdateDepth = FMath::Max(0, BulkUpdateDepth - 1);
	MarkRenderDataDirty();
}

bool UOptimizedSkeletalMeshWorldSubsystem::RemoveInstance(const FOptimizedSkeletalMeshInstanceHandle InHandle)
{
	return UnregisterInstance(InHandle);
}

bool UOptimizedSkeletalMeshWorldSubsystem::RemoveInstanceById(const int32 InInstanceId)
{
	return UnregisterInstance(FOptimizedSkeletalMeshInstanceHandle(InInstanceId));
}

int32 UOptimizedSkeletalMeshWorldSubsystem::RemoveInstances(
	const TArray<FOptimizedSkeletalMeshInstanceHandle>& InHandles)
{
	int32 removedCount = 0;
	++BulkUpdateDepth;
	for (const FOptimizedSkeletalMeshInstanceHandle& handle : InHandles)
	{
		if (UnregisterInstance(handle))
		{
			++removedCount;
		}
	}
	BulkUpdateDepth = FMath::Max(0, BulkUpdateDepth - 1);
	if (removedCount > 0)
	{
		MarkRenderDataDirty();
	}

	return removedCount;
}

int32 UOptimizedSkeletalMeshWorldSubsystem::RemoveInstancesById(const TArray<int32>& InInstanceIds)
{
	int32 removedCount = 0;
	++BulkUpdateDepth;
	for (const int32 instanceId : InInstanceIds)
	{
		if (UnregisterInstance(FOptimizedSkeletalMeshInstanceHandle(instanceId)))
		{
			++removedCount;
		}
	}
	BulkUpdateDepth = FMath::Max(0, BulkUpdateDepth - 1);
	if (removedCount > 0)
	{
		MarkRenderDataDirty();
	}

	return removedCount;
}

bool UOptimizedSkeletalMeshWorldSubsystem::UpdateInstance(
	FOptimizedSkeletalMeshInstanceHandle InHandle,
	const FOptimizedSkeletalMeshInstanceDesc& InDesc)
{
	if (!InDesc.SkeletalMesh)
	{
		return false;
	}

	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	*instance = InDesc;
	RefreshAnimationTracking(InHandle.Id, InDesc, true);
	MarkRenderDataDirty();

	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::UpdateInstanceTransform(
	FOptimizedSkeletalMeshInstanceHandle InHandle,
	const FTransform& InWorldTransform)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	if (instance->WorldTransform.Equals(InWorldTransform))
	{
		return true;
	}

	instance->WorldTransform = InWorldTransform;
	MarkTransformDirty(InHandle.Id);

	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::UpdateInstanceTransformById(
	const int32 InInstanceId,
	const FTransform& InWorldTransform)
{
	return UpdateInstanceTransform(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), InWorldTransform);
}

int32 UOptimizedSkeletalMeshWorldSubsystem::UpdateInstancesTransform(
	const TArray<FOptimizedSkeletalMeshInstanceHandle>& InHandles,
	const TArray<FTransform>& InWorldTransforms)
{
	const int32 count = FMath::Min(InHandles.Num(), InWorldTransforms.Num());
	int32 updatedCount = 0;
	for (int32 index = 0; index < count; ++index)
	{
		if (UpdateInstanceTransform(InHandles[index], InWorldTransforms[index]))
		{
			++updatedCount;
		}
	}

	return updatedCount;
}

int32 UOptimizedSkeletalMeshWorldSubsystem::UpdateInstancesTransformById(
	const TArray<int32>& InInstanceIds,
	const TArray<FTransform>& InWorldTransforms)
{
	const int32 count = FMath::Min(InInstanceIds.Num(), InWorldTransforms.Num());
	int32 updatedCount = 0;
	for (int32 index = 0; index < count; ++index)
	{
		if (UpdateInstanceTransformById(InInstanceIds[index], InWorldTransforms[index]))
		{
			++updatedCount;
		}
	}

	return updatedCount;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceLocation(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	const FVector& InWorldLocation)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	FTransform transform = instance->WorldTransform;
	transform.SetLocation(InWorldLocation);
	return UpdateInstanceTransform(InHandle, transform);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceLocationById(
	const int32 InInstanceId,
	const FVector& InWorldLocation)
{
	return SetInstanceLocation(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), InWorldLocation);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceRotation(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	const FRotator& InWorldRotation)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	FTransform transform = instance->WorldTransform;
	transform.SetRotation(InWorldRotation.Quaternion());
	return UpdateInstanceTransform(InHandle, transform);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceRotationById(
	const int32 InInstanceId,
	const FRotator& InWorldRotation)
{
	return SetInstanceRotation(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), InWorldRotation);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceScale(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	const FVector& InWorldScale3D)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	FTransform transform = instance->WorldTransform;
	transform.SetScale3D(InWorldScale3D);
	return UpdateInstanceTransform(InHandle, transform);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceScaleById(
	const int32 InInstanceId,
	const FVector& InWorldScale3D)
{
	return SetInstanceScale(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), InWorldScale3D);
}

bool UOptimizedSkeletalMeshWorldSubsystem::GetInstanceTransform(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	FTransform& OutWorldTransform) const
{
	const FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	OutWorldTransform = instance->WorldTransform;
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::GetInstanceTransformById(
	const int32 InInstanceId,
	FTransform& OutWorldTransform) const
{
	return GetInstanceTransform(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), OutWorldTransform);
}

bool UOptimizedSkeletalMeshWorldSubsystem::GetInstanceLocation(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	FVector& OutWorldLocation) const
{
	const FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	OutWorldLocation = instance->WorldTransform.GetLocation();
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::GetInstanceLocationById(
	const int32 InInstanceId,
	FVector& OutWorldLocation) const
{
	return GetInstanceLocation(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), OutWorldLocation);
}

bool UOptimizedSkeletalMeshWorldSubsystem::GetInstanceRotation(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	FRotator& OutWorldRotation) const
{
	const FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	OutWorldRotation = instance->WorldTransform.GetRotation().Rotator();
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::GetInstanceRotationById(
	const int32 InInstanceId,
	FRotator& OutWorldRotation) const
{
	return GetInstanceRotation(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), OutWorldRotation);
}

bool UOptimizedSkeletalMeshWorldSubsystem::GetInstanceScale(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	FVector& OutWorldScale3D) const
{
	const FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	OutWorldScale3D = instance->WorldTransform.GetScale3D();
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::GetInstanceScaleById(
	const int32 InInstanceId,
	FVector& OutWorldScale3D) const
{
	return GetInstanceScale(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), OutWorldScale3D);
}

bool UOptimizedSkeletalMeshWorldSubsystem::UpdateInstanceAnimationTime(
	FOptimizedSkeletalMeshInstanceHandle InHandle,
	const float InAnimationTime)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	instance->AnimationTime = FMath::Max(0.0f, InAnimationTime);
	RefreshAnimationTracking(InHandle.Id, *instance, true);
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceAnimationPlaying(
	FOptimizedSkeletalMeshInstanceHandle InHandle,
	const bool bInPlaying)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	if (instance->bPlayAnimation != bInPlaying)
	{
		instance->bPlayAnimation = bInPlaying;
		RefreshAnimationTracking(InHandle.Id, *instance, true);
	}
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceAnimationUpdateRateHz(
	FOptimizedSkeletalMeshInstanceHandle InHandle,
	const float InAnimationUpdateRateHz)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	instance->AnimationUpdateRateHz = FMath::Max(0.0f, InAnimationUpdateRateHz);
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceVisible(
	FOptimizedSkeletalMeshInstanceHandle InHandle,
	const bool bInVisible)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	if (instance->bVisible != bInVisible)
	{
		instance->bVisible = bInVisible;
		MarkRenderDataDirty();
	}

	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::ShowInstance(const FOptimizedSkeletalMeshInstanceHandle InHandle)
{
	return SetInstanceVisible(InHandle, true);
}

bool UOptimizedSkeletalMeshWorldSubsystem::HideInstance(const FOptimizedSkeletalMeshInstanceHandle InHandle)
{
	return SetInstanceVisible(InHandle, false);
}

int32 UOptimizedSkeletalMeshWorldSubsystem::SetInstancesVisible(
	const TArray<FOptimizedSkeletalMeshInstanceHandle>& InHandles,
	const bool bInVisible)
{
	int32 updatedCount = 0;

	for (const FOptimizedSkeletalMeshInstanceHandle& handle : InHandles)
	{
		FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(handle.Id);
		if (!instance || instance->bVisible == bInVisible)
		{
			continue;
		}

		instance->bVisible = bInVisible;
		++updatedCount;
	}

	if (updatedCount > 0)
	{
		MarkRenderDataDirty();
	}

	return updatedCount;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceCustomDepth(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	const bool bInRenderCustomDepth,
	const int32 InCustomDepthStencilValue)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	const int32 stencilValue = FMath::Clamp(InCustomDepthStencilValue, 0, 255);
	if (instance->bRenderCustomDepth == bInRenderCustomDepth
		&& instance->CustomDepthStencilValue == stencilValue)
	{
		return true;
	}

	instance->bRenderCustomDepth = bInRenderCustomDepth;
	instance->CustomDepthStencilValue = stencilValue;
	MarkBonePaletteDirty(InHandle.Id);
	MarkCustomDepthRenderDataDirty();
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceCustomDepthById(
	const int32 InInstanceId,
	const bool bInRenderCustomDepth,
	const int32 InCustomDepthStencilValue)
{
	return SetInstanceCustomDepth(
		FOptimizedSkeletalMeshInstanceHandle(InInstanceId),
		bInRenderCustomDepth,
		InCustomDepthStencilValue);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceRenderCustomDepth(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	const bool bInRenderCustomDepth)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	return SetInstanceCustomDepth(InHandle, bInRenderCustomDepth, instance->CustomDepthStencilValue);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceRenderCustomDepthById(
	const int32 InInstanceId,
	const bool bInRenderCustomDepth)
{
	return SetInstanceRenderCustomDepth(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), bInRenderCustomDepth);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceCustomDepthStencilValue(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	const int32 InCustomDepthStencilValue)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	return SetInstanceCustomDepth(InHandle, instance->bRenderCustomDepth, InCustomDepthStencilValue);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceCustomDepthStencilValueById(
	const int32 InInstanceId,
	const int32 InCustomDepthStencilValue)
{
	return SetInstanceCustomDepthStencilValue(
		FOptimizedSkeletalMeshInstanceHandle(InInstanceId),
		InCustomDepthStencilValue);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterial(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	UMaterialInterface* InMaterial)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	if (instance->MaterialOverride == InMaterial)
	{
		return true;
	}

	instance->MaterialOverride = InMaterial;
	MarkRenderDataDirty();
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterialById(
	const int32 InInstanceId,
	UMaterialInterface* InMaterial)
{
	return SetInstanceMaterial(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), InMaterial);
}

static bool SetMaterialCustomDataValue(FOptimizedSkeletalMeshInstanceDesc& InOutDesc, const int32 InParamIndex, const float InValue)
{
	if (InParamIndex < 0 || InParamIndex > 7)
	{
		return false;
	}

	FVector4f& customData = InParamIndex < 4 ? InOutDesc.MaterialCustomData0 : InOutDesc.MaterialCustomData1;
	const int32 vectorIndex = InParamIndex < 4 ? InParamIndex : InParamIndex - 4;
	if (FMath::IsNearlyEqual(customData[vectorIndex], InValue))
	{
		return true;
	}

	customData[vectorIndex] = InValue;
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterialScalarParam(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	const int32 InParamIndex,
	const float InValue)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	if (!SetMaterialCustomDataValue(*instance, InParamIndex, InValue))
	{
		return false;
	}

	MarkTransformDirty(InHandle.Id);
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterialScalarParamById(
	const int32 InInstanceId,
	const int32 InParamIndex,
	const float InValue)
{
	return SetInstanceMaterialScalarParam(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), InParamIndex, InValue);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterialVectorParam(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	const int32 InStartParamIndex,
	const FVector& InValue)
{
	if (InStartParamIndex < 0 || InStartParamIndex > 5)
	{
		return false;
	}

	const bool bX = SetInstanceMaterialScalarParam(InHandle, InStartParamIndex + 0, InValue.X);
	const bool bY = SetInstanceMaterialScalarParam(InHandle, InStartParamIndex + 1, InValue.Y);
	const bool bZ = SetInstanceMaterialScalarParam(InHandle, InStartParamIndex + 2, InValue.Z);
	return bX && bY && bZ;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterialVectorParamById(
	const int32 InInstanceId,
	const int32 InStartParamIndex,
	const FVector& InValue)
{
	return SetInstanceMaterialVectorParam(
		FOptimizedSkeletalMeshInstanceHandle(InInstanceId),
		InStartParamIndex,
		InValue);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterialBoolParam(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	const int32 InParamIndex,
	const bool bInValue)
{
	return SetInstanceMaterialScalarParam(InHandle, InParamIndex, bInValue ? 1.0f : 0.0f);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterialBoolParamById(
	const int32 InInstanceId,
	const int32 InParamIndex,
	const bool bInValue)
{
	return SetInstanceMaterialBoolParam(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), InParamIndex, bInValue);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterialTextureParam(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	const FName InParameterName,
	UTexture2D* InTexture)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance || InParameterName.IsNone() || !InTexture)
	{
		return false;
	}

	UMaterialInstanceDynamic* materialMid = Cast<UMaterialInstanceDynamic>(instance->MaterialOverride.Get());
	if (!materialMid)
	{
		UMaterialInterface* baseMaterial = instance->MaterialOverride;
		if (!baseMaterial && instance->SkeletalMesh)
		{
			const TArray<FSkeletalMaterial>& materials = instance->SkeletalMesh->GetMaterials();
			baseMaterial = materials.IsEmpty() ? nullptr : materials[0].MaterialInterface;
		}
		if (!baseMaterial)
		{
			return false;
		}

		materialMid = UMaterialInstanceDynamic::Create(baseMaterial, this);
		if (!materialMid)
		{
			return false;
		}

		if (!SetInstanceMaterial(InHandle, materialMid))
		{
			return false;
		}
	}

	materialMid->SetTextureParameterValue(InParameterName, InTexture);
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterialTextureParamById(
	const int32 InInstanceId,
	const FName InParameterName,
	UTexture2D* InTexture)
{
	return SetInstanceMaterialTextureParam(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), InParameterName, InTexture);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterialScalarParamByName(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	const FName InParameterName,
	const float InValue)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance || InParameterName.IsNone())
	{
		return false;
	}

	UMaterialInstanceDynamic* materialMid = Cast<UMaterialInstanceDynamic>(instance->MaterialOverride.Get());
	if (!materialMid)
	{
		UMaterialInterface* baseMaterial = instance->MaterialOverride;
		if (!baseMaterial && instance->SkeletalMesh)
		{
			const TArray<FSkeletalMaterial>& materials = instance->SkeletalMesh->GetMaterials();
			baseMaterial = materials.IsEmpty() ? nullptr : materials[0].MaterialInterface;
		}
		if (!baseMaterial)
		{
			return false;
		}

		materialMid = UMaterialInstanceDynamic::Create(baseMaterial, this);
		if (!materialMid)
		{
			return false;
		}

		if (!SetInstanceMaterial(InHandle, materialMid))
		{
			return false;
		}
	}

	materialMid->SetScalarParameterValue(InParameterName, InValue);
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterialScalarParamByNameId(
	const int32 InInstanceId,
	const FName InParameterName,
	const float InValue)
{
	return SetInstanceMaterialScalarParamByName(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), InParameterName, InValue);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterialVectorParamByName(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	const FName InParameterName,
	const FVector& InValue)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance || InParameterName.IsNone())
	{
		return false;
	}

	UMaterialInstanceDynamic* materialMid = Cast<UMaterialInstanceDynamic>(instance->MaterialOverride.Get());
	if (!materialMid)
	{
		UMaterialInterface* baseMaterial = instance->MaterialOverride;
		if (!baseMaterial && instance->SkeletalMesh)
		{
			const TArray<FSkeletalMaterial>& materials = instance->SkeletalMesh->GetMaterials();
			baseMaterial = materials.IsEmpty() ? nullptr : materials[0].MaterialInterface;
		}
		if (!baseMaterial)
		{
			return false;
		}

		materialMid = UMaterialInstanceDynamic::Create(baseMaterial, this);
		if (!materialMid)
		{
			return false;
		}

		if (!SetInstanceMaterial(InHandle, materialMid))
		{
			return false;
		}
	}

	materialMid->SetVectorParameterValue(InParameterName, FLinearColor(InValue.X, InValue.Y, InValue.Z, 1.0f));
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterialVectorParamByNameId(
	const int32 InInstanceId,
	const FName InParameterName,
	const FVector& InValue)
{
	return SetInstanceMaterialVectorParamByName(
		FOptimizedSkeletalMeshInstanceHandle(InInstanceId),
		InParameterName,
		InValue);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterialBoolParamByName(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	const FName InParameterName,
	const bool bInValue)
{
	return SetInstanceMaterialScalarParamByName(InHandle, InParameterName, bInValue ? 1.0f : 0.0f);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterialBoolParamByNameId(
	const int32 InInstanceId,
	const FName InParameterName,
	const bool bInValue)
{
	return SetInstanceMaterialBoolParamByName(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), InParameterName, bInValue);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterialColorParam(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	const int32 InStartParamIndex,
	const FLinearColor& InValue)
{
	const bool bX = SetInstanceMaterialScalarParam(InHandle, InStartParamIndex + 0, InValue.R);
	const bool bY = SetInstanceMaterialScalarParam(InHandle, InStartParamIndex + 1, InValue.G);
	const bool bZ = SetInstanceMaterialScalarParam(InHandle, InStartParamIndex + 2, InValue.B);
	const bool bW = SetInstanceMaterialScalarParam(InHandle, InStartParamIndex + 3, InValue.A);
	return bX && bY && bZ && bW;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterialColorParamById(
	const int32 InInstanceId,
	const int32 InStartParamIndex,
	const FLinearColor& InValue)
{
	return SetInstanceMaterialColorParam(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), InStartParamIndex, InValue);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterialColorParamByName(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	const FName InParameterName,
	const FLinearColor& InValue)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance || InParameterName.IsNone())
	{
		return false;
	}

	UMaterialInstanceDynamic* materialMid = Cast<UMaterialInstanceDynamic>(instance->MaterialOverride.Get());
	if (!materialMid)
	{
		UMaterialInterface* baseMaterial = instance->MaterialOverride;
		if (!baseMaterial && instance->SkeletalMesh)
		{
			const TArray<FSkeletalMaterial>& materials = instance->SkeletalMesh->GetMaterials();
			baseMaterial = materials.IsEmpty() ? nullptr : materials[0].MaterialInterface;
		}
		if (!baseMaterial)
		{
			return false;
		}

		materialMid = UMaterialInstanceDynamic::Create(baseMaterial, this);
		if (!materialMid)
		{
			return false;
		}

		if (!SetInstanceMaterial(InHandle, materialMid))
		{
			return false;
		}
	}

	materialMid->SetVectorParameterValue(InParameterName, InValue);
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceMaterialColorParamByNameId(
	const int32 InInstanceId,
	const FName InParameterName,
	const FLinearColor& InValue)
{
	return SetInstanceMaterialColorParamByName(
		FOptimizedSkeletalMeshInstanceHandle(InInstanceId),
		InParameterName,
		InValue);
}

bool UOptimizedSkeletalMeshWorldSubsystem::GetInstance(
	FOptimizedSkeletalMeshInstanceHandle InHandle,
	FOptimizedSkeletalMeshInstanceDesc& OutDesc) const
{
	const FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	OutDesc = *instance;
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::GetInstanceById(
	const int32 InInstanceId,
	FOptimizedSkeletalMeshInstanceDesc& OutDesc) const
{
	return GetInstance(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), OutDesc);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceAnimationAsset(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	UAnimSequence* InAnimation,
	const bool bInResetTime)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance || !instance->SkeletalMesh)
	{
		return false;
	}

	if (InAnimation)
	{
		const USkeleton* meshSkeleton = instance->SkeletalMesh->GetSkeleton();
		const USkeleton* animationSkeleton = InAnimation->GetSkeleton();
		if (!meshSkeleton || !animationSkeleton || meshSkeleton != animationSkeleton)
		{
			return false;
		}
	}

	instance->Animation = InAnimation;
	if (bInResetTime)
	{
		instance->AnimationTime = 0.0f;
	}

	if (!InAnimation)
	{
		instance->bPlayAnimation = false;
	}

	RefreshAnimationTracking(InHandle.Id, *instance, true);
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceAnimationAssetById(
	const int32 InInstanceId,
	UAnimSequence* InAnimation,
	const bool bInResetTime)
{
	return SetInstanceAnimationAsset(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), InAnimation, bInResetTime);
}

bool UOptimizedSkeletalMeshWorldSubsystem::PlayInstanceAnimation(const FOptimizedSkeletalMeshInstanceHandle InHandle)
{
	return SetInstanceAnimationPlaying(InHandle, true);
}

bool UOptimizedSkeletalMeshWorldSubsystem::PlayInstanceAnimationById(const int32 InInstanceId)
{
	return PlayInstanceAnimation(FOptimizedSkeletalMeshInstanceHandle(InInstanceId));
}

bool UOptimizedSkeletalMeshWorldSubsystem::PauseInstanceAnimation(const FOptimizedSkeletalMeshInstanceHandle InHandle)
{
	return SetInstanceAnimationPlaying(InHandle, false);
}

bool UOptimizedSkeletalMeshWorldSubsystem::PauseInstanceAnimationById(const int32 InInstanceId)
{
	return PauseInstanceAnimation(FOptimizedSkeletalMeshInstanceHandle(InInstanceId));
}

bool UOptimizedSkeletalMeshWorldSubsystem::StopInstanceAnimation(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	const bool bInResetTime)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	instance->bPlayAnimation = false;
	if (bInResetTime)
	{
		instance->AnimationTime = 0.0f;
	}

	RefreshAnimationTracking(InHandle.Id, *instance, true);
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::StopInstanceAnimationById(
	const int32 InInstanceId,
	const bool bInResetTime)
{
	return StopInstanceAnimation(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), bInResetTime);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceAnimationLooping(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	const bool bInLoopAnimation)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	instance->bLoopAnimation = bInLoopAnimation;
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceAnimationLoopingById(
	const int32 InInstanceId,
	const bool bInLoopAnimation)
{
	return SetInstanceAnimationLooping(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), bInLoopAnimation);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceAnimationPlayRate(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	const float InAnimationPlayRate)
{
	FOptimizedSkeletalMeshInstanceDesc* instance = Instances.Find(InHandle.Id);
	if (!instance)
	{
		return false;
	}

	instance->AnimationPlayRate = InAnimationPlayRate;
	RefreshAnimationTracking(InHandle.Id, *instance, true);
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceAnimationPlayRateById(
	const int32 InInstanceId,
	const float InAnimationPlayRate)
{
	return SetInstanceAnimationPlayRate(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), InAnimationPlayRate);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceAnimationTime(
	const FOptimizedSkeletalMeshInstanceHandle InHandle,
	const float InAnimationTime)
{
	return UpdateInstanceAnimationTime(InHandle, InAnimationTime);
}

bool UOptimizedSkeletalMeshWorldSubsystem::SetInstanceAnimationTimeById(
	const int32 InInstanceId,
	const float InAnimationTime)
{
	return SetInstanceAnimationTime(FOptimizedSkeletalMeshInstanceHandle(InInstanceId), InAnimationTime);
}

void UOptimizedSkeletalMeshWorldSubsystem::GetInstancesSnapshot(
	TArray<FOptimizedSkeletalMeshInstanceSnapshot>& OutInstances) const
{
	OutInstances.Reset(Instances.Num());

	for (const TPair<int32, FOptimizedSkeletalMeshInstanceDesc>& pair : Instances)
	{
		FOptimizedSkeletalMeshInstanceSnapshot& snapshot = OutInstances.AddDefaulted_GetRef();
		snapshot.Handle = FOptimizedSkeletalMeshInstanceHandle(pair.Key);
		snapshot.Desc = pair.Value;
	}
}

int32 UOptimizedSkeletalMeshWorldSubsystem::GetInstanceCount() const
{
	return Instances.Num();
}

int32 UOptimizedSkeletalMeshWorldSubsystem::GetVisibleRenderBatchCount() const
{
	if (!bVisibleRenderBatchCountDirty)
	{
		return CachedVisibleRenderBatchCount;
	}

	TSet<USkeletalMesh*> visibleMeshes;

	for (const TPair<int32, FOptimizedSkeletalMeshInstanceDesc>& pair : Instances)
	{
		const FOptimizedSkeletalMeshInstanceDesc& desc = pair.Value;
		if (!desc.bVisible || !desc.SkeletalMesh)
		{
			continue;
		}

		visibleMeshes.Add(desc.SkeletalMesh.Get());
	}

	CachedVisibleRenderBatchCount = visibleMeshes.Num();
	bVisibleRenderBatchCountDirty = false;
	return CachedVisibleRenderBatchCount;
}

FOptimizedSkeletalMeshAnimationStats UOptimizedSkeletalMeshWorldSubsystem::GetLastAnimationStats() const
{
	return LastAnimationStats;
}

FOptimizedSkeletalMeshRenderStats UOptimizedSkeletalMeshWorldSubsystem::GetLastRenderStats() const
{
	return LastRenderStats;
}

void UOptimizedSkeletalMeshWorldSubsystem::ApplyRenderSettings(const FOptimizedSkeletalMeshRenderSettings& InSettings)
{
	CurrentRenderSettings = OptimizedSkeletalMesh::NormalizeRenderSettings(InSettings);
	OptimizedSkeletalMesh::PushRenderSettingsToCVars(CurrentRenderSettings);
	RefreshActiveRenderSettings(true);
}

FOptimizedSkeletalMeshRenderSettings UOptimizedSkeletalMeshWorldSubsystem::GetRenderSettings() const
{
	return OptimizedSkeletalMesh::ResolveRenderSettingsFromCVars();
}

void UOptimizedSkeletalMeshWorldSubsystem::ReloadRenderSettingsFromCVars()
{
	RefreshActiveRenderSettings(true);
}

void UOptimizedSkeletalMeshWorldSubsystem::RegisterExternalRenderComponent(
	UOptimizedSkeletalMeshRenderComponent* InComponent)
{
	if (!InComponent)
	{
		return;
	}

	for (int32 componentIndex = ExternalRenderComponents.Num() - 1; componentIndex >= 0; --componentIndex)
	{
		const UOptimizedSkeletalMeshRenderComponent* component = ExternalRenderComponents[componentIndex].Get();
		if (!component || component == InComponent)
		{
			ExternalRenderComponents.RemoveAtSwap(componentIndex, 1, EAllowShrinking::No);
		}
	}

	ExternalRenderComponents.Add(InComponent);
	ApplyRenderSettingsToComponent(InComponent);
}

void UOptimizedSkeletalMeshWorldSubsystem::UnregisterExternalRenderComponent(
	UOptimizedSkeletalMeshRenderComponent* InComponent)
{
	for (int32 componentIndex = ExternalRenderComponents.Num() - 1; componentIndex >= 0; --componentIndex)
	{
		const UOptimizedSkeletalMeshRenderComponent* component = ExternalRenderComponents[componentIndex].Get();
		if (!component || component == InComponent)
		{
			ExternalRenderComponents.RemoveAtSwap(componentIndex, 1, EAllowShrinking::No);
		}
	}
}

void UOptimizedSkeletalMeshWorldSubsystem::ApplyRenderSettingsToComponent(UOptimizedSkeletalMeshRenderComponent* InComponent) const
{
	if (!InComponent)
	{
		return;
	}

	InComponent->SetDrawDebugBounds(ActiveRenderSettings.bDrawDebugBounds);
	InComponent->SetDrawMeshSections(ActiveRenderSettings.bDrawMeshSections);
	InComponent->SetMeshDrawMode(ActiveRenderSettings.MeshDrawMode);
	InComponent->SetMaxMeshDrawInstances(0);
	InComponent->SetInstanceFrustumCulling(ActiveRenderSettings.bEnableInstanceFrustumCulling);
	InComponent->SetInstanceCullBoundsScale(ActiveRenderSettings.InstanceCullBoundsScale);
	InComponent->SetConservativeProxyBounds(ActiveRenderSettings.bUseConservativeProxyBounds);
	InComponent->SetConservativeProxyBoundsExtent(ActiveRenderSettings.ConservativeProxyBoundsExtent);
	InComponent->SetDrawCullingDebug(ActiveRenderSettings.bDrawCullingDebug);
	InComponent->SetDrawCullTestBounds(ActiveRenderSettings.bDrawCullTestBounds);
	InComponent->SetCastShadows(ActiveRenderSettings.bCastShadows);
	InComponent->SetNearFullShadowDistance(ActiveRenderSettings.NearFullShadowDistance);
	InComponent->SetMidShadowDistance(ActiveRenderSettings.MidShadowDistance);
	InComponent->SetMidShadowUpdateDivisor(ActiveRenderSettings.MidShadowUpdateDivisor);
	InComponent->SetFarShadowUpdateDivisor(ActiveRenderSettings.FarShadowUpdateDivisor);
	InComponent->SetMaxShadowCastDistance(ActiveRenderSettings.MaxShadowCastDistance);
	InComponent->SetMaxDynamicShadowCasters(ActiveRenderSettings.MaxDynamicShadowCasters);
	InComponent->SetNearShadowLodBias(ActiveRenderSettings.NearShadowLodBias);
	InComponent->SetMidShadowLodBias(ActiveRenderSettings.MidShadowLodBias);
	InComponent->SetFarShadowLodBias(ActiveRenderSettings.FarShadowLodBias);
	InComponent->SetMaxShadowSectionsPerLOD(ActiveRenderSettings.MaxShadowSectionsPerLOD);
	InComponent->SetLocalLightMaxShadowCastDistance(ActiveRenderSettings.LocalLightMaxShadowCastDistance);
	InComponent->SetLocalLightMaxDynamicShadowCasters(ActiveRenderSettings.LocalLightMaxDynamicShadowCasters);
	InComponent->SetLocalLightShadowLodBias(ActiveRenderSettings.LocalLightShadowLodBias);
	InComponent->SetLocalLightMaxShadowSectionsPerLOD(ActiveRenderSettings.LocalLightMaxShadowSectionsPerLOD);
}

const TArray<FMatrix44f>* UOptimizedSkeletalMeshWorldSubsystem::GetInstanceBonePalette(
	const FOptimizedSkeletalMeshInstanceHandle InHandle) const
{
	return InstanceBonePalettes.Find(InHandle.Id);
}

float UOptimizedSkeletalMeshWorldSubsystem::GetInstanceAnimationBlendAlpha(
	const FOptimizedSkeletalMeshInstanceHandle InHandle) const
{
	if (const float* blendAlpha = InstanceAnimationBlendAlphas.Find(InHandle.Id))
	{
		return FMath::Clamp(*blendAlpha, 0.0f, 1.0f);
	}

	return 1.0f;
}

void UOptimizedSkeletalMeshWorldSubsystem::GetBonePaletteSnapshots(
	TArray<FOptimizedSkeletalMeshBonePaletteSnapshot>& OutSnapshots) const
{
	OutSnapshots.Reset(InstanceBonePalettes.Num());

	for (const TPair<int32, TArray<FMatrix44f>>& pair : InstanceBonePalettes)
	{
		if (pair.Value.IsEmpty())
		{
			continue;
		}

		FOptimizedSkeletalMeshBonePaletteSnapshot& snapshot = OutSnapshots.AddDefaulted_GetRef();
		snapshot.Handle = FOptimizedSkeletalMeshInstanceHandle(pair.Key);
		if (const TArray<FMatrix44f>* previousBonePalette = PreviousInstanceBonePalettes.Find(pair.Key))
		{
			snapshot.PreviousBonePalette = *previousBonePalette;
		}
		else
		{
			snapshot.PreviousBonePalette = pair.Value;
		}

		snapshot.BonePalette = pair.Value;
		snapshot.BlendAlpha = GetInstanceAnimationBlendAlpha(FOptimizedSkeletalMeshInstanceHandle(pair.Key));
	}

	OutSnapshots.Sort(
		[](const FOptimizedSkeletalMeshBonePaletteSnapshot& InLeft, const FOptimizedSkeletalMeshBonePaletteSnapshot& InRight) {
			return InLeft.Handle.Id < InRight.Handle.Id;
		});
}

int32 UOptimizedSkeletalMeshWorldSubsystem::GetCachedBonePaletteCount() const
{
	return InstanceBonePalettes.Num();
}

void UOptimizedSkeletalMeshWorldSubsystem::UpdateRenderVisibleInstanceIds(
	const TConstArrayView<int32> InVisibleInstanceIds)
{
	TSet<int32> newVisibleInstanceIds;
	newVisibleInstanceIds.Reserve(InVisibleInstanceIds.Num());
	for (const int32 instanceId : InVisibleInstanceIds)
	{
		if (Instances.Contains(instanceId))
		{
			newVisibleInstanceIds.Add(instanceId);
		}
	}

	for (const int32 instanceId : newVisibleInstanceIds)
	{
		if (!RenderVisibleInstanceIds.Contains(instanceId) && InstanceBonePalettes.Contains(instanceId))
		{
			MarkBonePaletteDirty(instanceId);
		}

		if (!RenderVisibleInstanceIds.Contains(instanceId))
		{
			DirtyAnimationInstanceIds.Add(instanceId);
			AnimationUpdateAccumulators.Remove(instanceId);
		}
	}

	RenderVisibleInstanceIds = MoveTemp(newVisibleInstanceIds);
}

void UOptimizedSkeletalMeshWorldSubsystem::UpdateLastRenderStats(const FOptimizedSkeletalMeshRenderStats& InStats)
{
	LastRenderStats = InStats;
}

void UOptimizedSkeletalMeshWorldSubsystem::SetExternalRenderBridgeActive(const bool bInActive)
{
	bExternalRenderBridgeActive = bInActive;

	if (bExternalRenderBridgeActive)
	{
		DestroyRenderBridge();
	}
	else
	{
		EnsureRenderBridge();
	}
}

void UOptimizedSkeletalMeshWorldSubsystem::EnsureRenderBridge()
{
	if (bExternalRenderBridgeActive)
	{
		return;
	}

	if (RenderComponent && RenderComponent->IsRegistered())
	{
		return;
	}

	UWorld* world = GetWorld();
	if (!world || world->bIsTearingDown || world->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!RenderBridgeActor)
	{
		FActorSpawnParameters spawnParameters;
		spawnParameters.Name = MakeUniqueObjectName(
			world->PersistentLevel,
			AOptimizedSkeletalMeshRenderBridgeActor::StaticClass(),
			TEXT("OptimizedSkeletalMeshRenderBridge"));
		spawnParameters.ObjectFlags |= RF_Transient;
		spawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		RenderBridgeActor = world->SpawnActor<AOptimizedSkeletalMeshRenderBridgeActor>(spawnParameters);
		if (RenderBridgeActor)
		{
			RenderBridgeActor->SetCanBeDamaged(false);
			RenderBridgeActor->SetActorEnableCollision(false);
			RenderBridgeActor->SetActorHiddenInGame(false);
			RenderBridgeActor->SetHidden(false);
		}
	}

	if (RenderBridgeActor && !RenderComponent)
	{
		if (const AOptimizedSkeletalMeshRenderBridgeActor* bridgeActor =
				Cast<AOptimizedSkeletalMeshRenderBridgeActor>(RenderBridgeActor.Get()))
		{
			RenderComponent = bridgeActor->GetRenderComponent();
		}

		if (RenderComponent)
		{
			RenderComponent->SetOptimizedSkeletalMeshSubsystem(this);
			RenderComponent->SetHiddenInGame(false, true);
			RenderComponent->SetVisibility(true, true);
			if (!RenderComponent->IsRegistered())
			{
				RenderComponent->RegisterComponentWithWorld(world);
			}

			ApplyRenderSettingsToComponent(RenderComponent);
			RenderComponent->RequestRenderRefresh();
		}
	}
	else if (RenderBridgeActor && RenderComponent && !RenderComponent->IsRegistered())
	{
		RenderComponent->SetOptimizedSkeletalMeshSubsystem(this);
		if (RenderComponent->GetOwner() != RenderBridgeActor)
		{
			RenderComponent->Rename(nullptr, RenderBridgeActor);
			RenderBridgeActor->AddInstanceComponent(RenderComponent);
		}

		RenderBridgeActor->SetActorHiddenInGame(false);
		RenderBridgeActor->SetHidden(false);
		RenderComponent->SetHiddenInGame(false, true);
		RenderComponent->SetVisibility(true, true);
		RenderComponent->RegisterComponentWithWorld(world);
		ApplyRenderSettingsToComponent(RenderComponent);
		RenderComponent->RequestRenderRefresh();
	}
}

void UOptimizedSkeletalMeshWorldSubsystem::DestroyRenderBridge()
{
	for (TPair<int32, TObjectPtr<UOptimizedSkeletalMeshRenderComponent>>& pair : CustomDepthRenderComponents)
	{
		if (UOptimizedSkeletalMeshRenderComponent* component = pair.Value.Get())
		{
			component->DestroyComponent();
		}
	}
	CustomDepthRenderComponents.Reset();

	if (RenderComponent)
	{
		if (RenderComponent->IsRegistered())
		{
			RenderComponent->UnregisterComponent();
		}

		RenderComponent = nullptr;
	}

	if (RenderBridgeActor)
	{
		RenderBridgeActor->Destroy();
		RenderBridgeActor = nullptr;
	}
}

void UOptimizedSkeletalMeshWorldSubsystem::RefreshCustomDepthRenderComponents()
{
	if (bExternalRenderBridgeActive)
	{
		for (TPair<int32, TObjectPtr<UOptimizedSkeletalMeshRenderComponent>>& pair : CustomDepthRenderComponents)
		{
			if (UOptimizedSkeletalMeshRenderComponent* component = pair.Value.Get())
			{
				component->DestroyComponent();
			}
		}
		CustomDepthRenderComponents.Reset();
		return;
	}

	TSet<int32> neededStencilValues;
	for (const TPair<int32, FOptimizedSkeletalMeshInstanceDesc>& pair : Instances)
	{
		const FOptimizedSkeletalMeshInstanceDesc& desc = pair.Value;
		if (desc.bVisible && desc.bRenderCustomDepth && desc.SkeletalMesh)
		{
			neededStencilValues.Add(FMath::Clamp(desc.CustomDepthStencilValue, 0, 255));
		}
	}

	for (auto it = CustomDepthRenderComponents.CreateIterator(); it; ++it)
	{
		if (!neededStencilValues.Contains(it.Key()) || !it.Value())
		{
			if (UOptimizedSkeletalMeshRenderComponent* component = it.Value().Get())
			{
				component->DestroyComponent();
			}
			it.RemoveCurrent();
		}
	}

	if (neededStencilValues.IsEmpty())
	{
		bCustomDepthRenderDataDirty = false;
		return;
	}

	EnsureRenderBridge();

	UWorld* world = GetWorld();
	AActor* bridgeActor = RenderBridgeActor.Get();
	if (!world || !bridgeActor || world->bIsTearingDown || world->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	for (const int32 stencilValue : neededStencilValues)
	{
		UOptimizedSkeletalMeshRenderComponent* component = nullptr;
		bool bNewComponent = false;
		if (TObjectPtr<UOptimizedSkeletalMeshRenderComponent>* existingComponent =
				CustomDepthRenderComponents.Find(stencilValue))
		{
			component = existingComponent->Get();
		}

		if (!component)
		{
			const FName componentName = MakeUniqueObjectName(
				bridgeActor,
				UOptimizedSkeletalMeshRenderComponent::StaticClass(),
				*FString::Printf(TEXT("OptimizedSkeletalMeshCustomDepth_%03d"), stencilValue));
			component = NewObject<UOptimizedSkeletalMeshRenderComponent>(
				bridgeActor,
				UOptimizedSkeletalMeshRenderComponent::StaticClass(),
				componentName,
				RF_Transient);
			if (!component)
			{
				continue;
			}

			component->SetupAttachment(bridgeActor->GetRootComponent());
			bridgeActor->AddInstanceComponent(component);
			CustomDepthRenderComponents.Add(stencilValue, component);
			bNewComponent = true;
		}

		if (bNewComponent)
		{
			component->SetOptimizedSkeletalMeshSubsystem(this);
			component->SetHiddenInGame(false, true);
			component->SetVisibility(true, true);
			component->SetMobility(EComponentMobility::Movable);
			ApplyRenderSettingsToComponent(component);
			component->SetCustomDepthOnlyMode(true, stencilValue);
		}

		if (!component->IsRegistered())
		{
			component->RegisterComponentWithWorld(world);
		}
	}
}

void UOptimizedSkeletalMeshWorldSubsystem::RequestRenderRefreshForAllComponents()
{
	if (RenderComponent)
	{
		RenderComponent->RequestRenderRefresh();
	}

	RequestCustomDepthRenderRefresh();
}

void UOptimizedSkeletalMeshWorldSubsystem::RequestCustomDepthRenderRefresh()
{
	for (TPair<int32, TObjectPtr<UOptimizedSkeletalMeshRenderComponent>>& pair : CustomDepthRenderComponents)
	{
		if (UOptimizedSkeletalMeshRenderComponent* component = pair.Value.Get())
		{
			component->RequestRenderRefresh();
		}
	}
}

bool UOptimizedSkeletalMeshWorldSubsystem::PushBonePalettesToRenderComponents()
{
	bool bHasComponent = false;
	bool bPushedAll = true;

	if (RenderComponent)
	{
		bHasComponent = true;
		bPushedAll &= RenderComponent->PushBonePalettesToRenderThread();
	}

	for (TPair<int32, TObjectPtr<UOptimizedSkeletalMeshRenderComponent>>& pair : CustomDepthRenderComponents)
	{
		if (UOptimizedSkeletalMeshRenderComponent* component = pair.Value.Get())
		{
			bHasComponent = true;
			bPushedAll &= component->PushBonePalettesToRenderThread();
		}
	}

	return bHasComponent && bPushedAll;
}

bool UOptimizedSkeletalMeshWorldSubsystem::PushInstanceTransformsToRenderComponents()
{
	if (DirtyTransformInstanceIds.IsEmpty())
	{
		return true;
	}

	TArray<int32> dirtyInstanceIds;
	dirtyInstanceIds.Reserve(DirtyTransformInstanceIds.Num());
	for (const int32 instanceId : DirtyTransformInstanceIds)
	{
		dirtyInstanceIds.Add(instanceId);
	}

	bool bHasComponent = false;
	bool bPushedAll = true;
	if (RenderComponent)
	{
		bHasComponent = true;
		bPushedAll &= RenderComponent->PushInstanceTransformsToRenderThread(dirtyInstanceIds);
	}

	for (TPair<int32, TObjectPtr<UOptimizedSkeletalMeshRenderComponent>>& pair : CustomDepthRenderComponents)
	{
		if (UOptimizedSkeletalMeshRenderComponent* component = pair.Value.Get())
		{
			bHasComponent = true;
			bPushedAll &= component->PushInstanceTransformsToRenderThread(dirtyInstanceIds);
		}
	}

	return !bHasComponent || bPushedAll;
}

void UOptimizedSkeletalMeshWorldSubsystem::ApplyRenderSettingsToComponent()
{
	ApplyRenderSettingsToComponent(RenderComponent);

	for (int32 componentIndex = ExternalRenderComponents.Num() - 1; componentIndex >= 0; --componentIndex)
	{
		UOptimizedSkeletalMeshRenderComponent* component = ExternalRenderComponents[componentIndex].Get();
		if (!component)
		{
			ExternalRenderComponents.RemoveAtSwap(componentIndex, 1, EAllowShrinking::No);
			continue;
		}

		ApplyRenderSettingsToComponent(component);
	}

	for (TPair<int32, TObjectPtr<UOptimizedSkeletalMeshRenderComponent>>& pair : CustomDepthRenderComponents)
	{
		if (UOptimizedSkeletalMeshRenderComponent* component = pair.Value.Get())
		{
			ApplyRenderSettingsToComponent(component);
			component->SetCustomDepthOnlyMode(true, pair.Key);
		}
	}
}

void UOptimizedSkeletalMeshWorldSubsystem::RefreshActiveRenderSettings(const bool bInForce)
{
	const FOptimizedSkeletalMeshRenderSettings resolvedSettings = OptimizedSkeletalMesh::ResolveRenderSettingsFromCVars();
	CurrentRenderSettings = resolvedSettings;

	if (!bInForce && OptimizedSkeletalMesh::AreRenderSettingsEqual(ActiveRenderSettings, resolvedSettings))
	{
		return;
	}

	ActiveRenderSettings = resolvedSettings;
	ApplyRenderSettingsToComponent();
	MarkRenderDataDirty();
}

int32 UOptimizedSkeletalMeshWorldSubsystem::AllocateInstanceId()
{
	if (!FreeInstanceIds.IsEmpty())
	{
		return FreeInstanceIds.Pop(EAllowShrinking::No);
	}

	return NextInstanceId++;
}

bool UOptimizedSkeletalMeshWorldSubsystem::IsValidInstanceId(const int32 InInstanceId) const
{
	return Instances.Contains(InInstanceId);
}

void UOptimizedSkeletalMeshWorldSubsystem::MarkRenderDataDirty()
{
	bRenderDataDirty = true;
	bCustomDepthRenderDataDirty = true;
	bVisibleRenderBatchCountDirty = true;

	if (BulkUpdateDepth <= 0)
	{
		RefreshCustomDepthRenderComponents();
		RequestRenderRefreshForAllComponents();
	}
}

void UOptimizedSkeletalMeshWorldSubsystem::MarkCustomDepthRenderDataDirty()
{
	bCustomDepthRenderDataDirty = true;

	if (BulkUpdateDepth <= 0)
	{
		RefreshCustomDepthRenderComponents();
		RequestCustomDepthRenderRefresh();
		if (HasDirtyRenderVisibleBonePalettes() && PushBonePalettesToRenderComponents())
		{
			ClearDirtyRenderVisibleBonePalettes();
		}
	}
}

void UOptimizedSkeletalMeshWorldSubsystem::MarkTransformDirty(const int32 InInstanceId)
{
	DirtyTransformInstanceIds.Add(InInstanceId);

	if (BulkUpdateDepth <= 0 && !bRenderDataDirty)
	{
		if (PushInstanceTransformsToRenderComponents())
		{
			ClearDirtyTransforms();
		}
	}
}

bool UOptimizedSkeletalMeshWorldSubsystem::HasDirtyTransforms() const
{
	return !DirtyTransformInstanceIds.IsEmpty();
}

void UOptimizedSkeletalMeshWorldSubsystem::ClearDirtyTransforms()
{
	DirtyTransformInstanceIds.Reset();
}

void UOptimizedSkeletalMeshWorldSubsystem::RefreshAnimationTracking(
	const int32 InInstanceId,
	const FOptimizedSkeletalMeshInstanceDesc& InDesc,
	const bool bInForceDirty)
{
	if (!InDesc.Animation || !InDesc.SkeletalMesh)
	{
		RemoveAnimationTracking(InInstanceId);
		if (InstanceBonePalettes.Remove(InInstanceId) > 0)
		{
			MarkBonePaletteDirty(InInstanceId);
		}
		return;
	}

	if (ShouldTickAnimation(InDesc))
	{
		ActiveAnimationInstanceIds.Add(InInstanceId);
	}
	else
	{
		ActiveAnimationInstanceIds.Remove(InInstanceId);
	}

	if (bInForceDirty)
	{
		DirtyAnimationInstanceIds.Add(InInstanceId);
	}
}

void UOptimizedSkeletalMeshWorldSubsystem::RemoveAnimationTracking(const int32 InInstanceId)
{
	ActiveAnimationInstanceIds.Remove(InInstanceId);
	DirtyAnimationInstanceIds.Remove(InInstanceId);
	DirtyBonePaletteInstanceIds.Remove(InInstanceId);
	DirtyTransformInstanceIds.Remove(InInstanceId);
	RenderVisibleInstanceIds.Remove(InInstanceId);
	PreviousInstanceBonePalettes.Remove(InInstanceId);
	InstanceAnimationBlendAlphas.Remove(InInstanceId);
	AnimationUpdateAccumulators.Remove(InInstanceId);
}

void UOptimizedSkeletalMeshWorldSubsystem::MarkBonePaletteDirty(const int32 InInstanceId)
{
	DirtyBonePaletteInstanceIds.Add(InInstanceId);
}

bool UOptimizedSkeletalMeshWorldSubsystem::HasDirtyRenderVisibleBonePalettes() const
{
	return !DirtyBonePaletteInstanceIds.IsEmpty();
}

void UOptimizedSkeletalMeshWorldSubsystem::ClearDirtyRenderVisibleBonePalettes()
{
	DirtyBonePaletteInstanceIds.Reset();
}

bool UOptimizedSkeletalMeshWorldSubsystem::ResolveNamedMaterialParamSlot(
	const FName InParameterName,
	const int32 InWidth,
	int32& OutStartIndex)
{
	OutStartIndex = INDEX_NONE;
	if (InParameterName.IsNone() || InWidth <= 0 || InWidth > 8)
	{
		return false;
	}

	if (const FIntPoint* existingSlot = NamedMaterialParamSlots.Find(InParameterName))
	{
		if (existingSlot->Y != InWidth)
		{
			return false;
		}

		OutStartIndex = existingSlot->X;
		return true;
	}

	if (NextNamedMaterialParamSlot + InWidth > 8)
	{
		return false;
	}

	const int32 startIndex = NextNamedMaterialParamSlot;
	NamedMaterialParamSlots.Add(InParameterName, FIntPoint(startIndex, InWidth));
	NextNamedMaterialParamSlot += InWidth;
	OutStartIndex = startIndex;
	return true;
}

float UOptimizedSkeletalMeshWorldSubsystem::GetEffectiveAnimationUpdateRateHz(
	const FOptimizedSkeletalMeshInstanceDesc& InDesc,
	const float InNearestCameraDistance) const
{
	const UOptimizedSkeletalMeshSettings* settings = GetDefault<UOptimizedSkeletalMeshSettings>();
	if (!settings
		|| settings->DistanceBasedRateMode == EOptimizedSkeletalMeshDistanceBasedRateMode::Static
		|| InNearestCameraDistance < 0.0f)
	{
		return InDesc.AnimationUpdateRateHz;
	}

	const float baseUpdateRateHz = InDesc.AnimationUpdateRateHz > 0.0f
		? InDesc.AnimationUpdateRateHz
		: settings->DefaultAnimationUpdateRateHz;
	if (baseUpdateRateHz <= 0.0f)
	{
		return 0.0f;
	}

	const float updateRateScale = GetUpdateRateScaleForDistance(InNearestCameraDistance);
	const float scaledUpdateRateHz = baseUpdateRateHz * updateRateScale;
	return FMath::Max(settings->MinAnimationUpdateRateHz, scaledUpdateRateHz);
}

float UOptimizedSkeletalMeshWorldSubsystem::GetUpdateRateScaleForDistance(const float InDistance)
{
	const UOptimizedSkeletalMeshSettings* settings = GetDefault<UOptimizedSkeletalMeshSettings>();
	if (!settings)
	{
		return 1.0f;
	}

	if (settings->DistanceBasedRateMode == EOptimizedSkeletalMeshDistanceBasedRateMode::Static)
	{
		return 1.0f;
	}

	if (settings->DistanceBasedRateMode == EOptimizedSkeletalMeshDistanceBasedRateMode::DistanceBasedCurve)
	{
		const UCurveFloat* updateRateCurve = settings->DistanceUpdateRateCurve.LoadSynchronous();
		return updateRateCurve ? FMath::Max(0.0f, updateRateCurve->GetFloatValue(InDistance)) : 1.0f;
	}

	if (settings->DistanceUpdateRateBands.IsEmpty())
	{
		return 1.0f;
	}

	float selectedScale = settings->DistanceUpdateRateBands[0].UpdateRateScale;
	float selectedDistance = settings->DistanceUpdateRateBands[0].Distance;
	bool bFoundBand = false;

	for (const FOptimizedSkeletalMeshDistanceUpdateRateBand& band : settings->DistanceUpdateRateBands)
	{
		if (band.Distance >= InDistance)
		{
			if (!bFoundBand || band.Distance < selectedDistance)
			{
				selectedDistance = band.Distance;
				selectedScale = band.UpdateRateScale;
				bFoundBand = true;
			}
		}
		else if (!bFoundBand && band.Distance >= selectedDistance)
		{
			selectedDistance = band.Distance;
			selectedScale = band.UpdateRateScale;
		}
	}

	return FMath::Max(0.0f, selectedScale);
}

bool UOptimizedSkeletalMeshWorldSubsystem::GetNearestCameraDistance(
	const FVector& InWorldLocation,
	float& OutDistance) const
{
	const UWorld* world = GetWorld();
	if (!world)
	{
		return false;
	}

	float nearestDistanceSquared = TNumericLimits<float>::Max();
	bool bFoundCamera = false;
	for (FConstPlayerControllerIterator controllerIterator = world->GetPlayerControllerIterator(); controllerIterator; ++controllerIterator)
	{
		const APlayerController* playerController = controllerIterator->Get();
		if (!playerController)
		{
			continue;
		}

		FVector cameraLocation = FVector::ZeroVector;
		FRotator cameraRotation = FRotator::ZeroRotator;
		playerController->GetPlayerViewPoint(cameraLocation, cameraRotation);
		nearestDistanceSquared = FMath::Min(nearestDistanceSquared, FVector::DistSquared(InWorldLocation, cameraLocation));
		bFoundCamera = true;
	}

	if (!bFoundCamera)
	{
		return false;
	}

	OutDistance = FMath::Sqrt(nearestDistanceSquared);
	return true;
}

bool UOptimizedSkeletalMeshWorldSubsystem::ShouldTickAnimation(
	const FOptimizedSkeletalMeshInstanceDesc& InDesc)
{
	return InDesc.SkeletalMesh
		&& InDesc.Animation
		&& InDesc.bPlayAnimation
		&& !FMath::IsNearlyZero(InDesc.AnimationPlayRate);
}

void UOptimizedSkeletalMeshWorldSubsystem::TickAnimation(const float InDeltaTime)
{
	FOptimizedSkeletalMeshAnimationStats newStats;
	InitializeAnimationStats(newStats, InDeltaTime);
	if (ActiveAnimationInstanceIds.IsEmpty() && DirtyAnimationInstanceIds.IsEmpty())
	{
		FinalizeAnimationStats(newStats);
		LastAnimationStats = newStats;
		OptimizedSkeletalMesh::PublishAnimationStats(LastAnimationStats);
		return;
	}

	TArray<int32> instanceIdsToProcess;
	TArray<int32> dirtyInstanceIdsToProcess;
	BuildAnimationInstanceIdsToProcess(instanceIdsToProcess, dirtyInstanceIdsToProcess);

	TArray<FOptimizedSkeletalMeshAnimationEvaluationWork> evaluationWork;
	BuildAnimationEvaluationWork(InDeltaTime, instanceIdsToProcess, newStats, evaluationWork);

	TArray<FOptimizedSkeletalMeshAnimationEvaluationResult> evaluationResults;
	RunAnimationEvaluationWork(evaluationWork, newStats, evaluationResults);

	if (newStats.DistanceRateScaledInstances > 0)
	{
		newStats.AverageEffectiveUpdateRateHz /= static_cast<float>(newStats.DistanceRateScaledInstances);
	}

	ApplyAnimationEvaluationResults(evaluationResults, newStats);
	ClearProcessedDirtyAnimationIds(dirtyInstanceIdsToProcess);
	FinalizeAnimationStats(newStats);

	LastAnimationStats = newStats;
	OptimizedSkeletalMesh::PublishAnimationStats(LastAnimationStats);
}

void UOptimizedSkeletalMeshWorldSubsystem::InitializeAnimationStats(
	FOptimizedSkeletalMeshAnimationStats& OutStats,
	const float InDeltaTime) const
{
	OutStats = FOptimizedSkeletalMeshAnimationStats();
	OutStats.RegisteredInstances = Instances.Num();
	OutStats.LastDeltaTime = InDeltaTime;
	OutStats.ActiveAnimationInstances = ActiveAnimationInstanceIds.Num();
	OutStats.DirtyAnimationInstances = DirtyAnimationInstanceIds.Num();
	OutStats.RenderVisibleAnimatedInstances = RenderVisibleInstanceIds.Num();
}

void UOptimizedSkeletalMeshWorldSubsystem::FinalizeAnimationStats(FOptimizedSkeletalMeshAnimationStats& OutStats) const
{
	OutStats.BonePaletteInstances = InstanceBonePalettes.Num();
	for (const TPair<int32, TArray<FMatrix44f>>& pair : InstanceBonePalettes)
	{
		OutStats.TotalBoneMatrices += pair.Value.Num();
		OutStats.MaxBonesPerInstance = FMath::Max(OutStats.MaxBonesPerInstance, pair.Value.Num());
	}

	OutStats.ActiveAnimationInstances = ActiveAnimationInstanceIds.Num();
	OutStats.DirtyAnimationInstances = DirtyAnimationInstanceIds.Num();
	OutStats.RenderVisibleAnimatedInstances = RenderVisibleInstanceIds.Num();
	OutStats.DirtyCpuPaletteInstances = DirtyBonePaletteInstanceIds.Num();
	OutStats.DirtyGpuPaletteInstances = 0;
	OutStats.GpuPaletteUploadSkippedInstances = 0;
	for (const int32 instanceId : DirtyBonePaletteInstanceIds)
	{
		if (RenderVisibleInstanceIds.Contains(instanceId))
		{
			++OutStats.DirtyGpuPaletteInstances;
		}
		else
		{
			++OutStats.GpuPaletteUploadSkippedInstances;
		}
	}
}

void UOptimizedSkeletalMeshWorldSubsystem::BuildAnimationInstanceIdsToProcess(
	TArray<int32>& OutInstanceIdsToProcess,
	TArray<int32>& OutDirtyInstanceIdsToProcess) const
{
	OutInstanceIdsToProcess.Reset();
	OutDirtyInstanceIdsToProcess.Reset();
	OutInstanceIdsToProcess.Reserve(ActiveAnimationInstanceIds.Num() + DirtyAnimationInstanceIds.Num());

	const int32 maxDirtyEvaluationsPerFrame =
		OptimizedSkeletalMesh::CVarAnimationMaxDirtyEvaluationsPerFrame.GetValueOnGameThread();
	const int32 maxDirtyCount = maxDirtyEvaluationsPerFrame > 0
		? maxDirtyEvaluationsPerFrame
		: TNumericLimits<int32>::Max();
	OutDirtyInstanceIdsToProcess.Reserve(FMath::Min(DirtyAnimationInstanceIds.Num(), maxDirtyCount));
	for (const int32 instanceId : DirtyAnimationInstanceIds)
	{
		OutDirtyInstanceIdsToProcess.Add(instanceId);
		if (OutDirtyInstanceIdsToProcess.Num() >= maxDirtyCount)
		{
			break;
		}
	}

	for (const int32 instanceId : OutDirtyInstanceIdsToProcess)
	{
		OutInstanceIdsToProcess.AddUnique(instanceId);
	}

	for (const int32 instanceId : ActiveAnimationInstanceIds)
	{
		OutInstanceIdsToProcess.AddUnique(instanceId);
	}
}

bool UOptimizedSkeletalMeshWorldSubsystem::AdvanceAnimationTime(
	const int32 InInstanceId,
	FOptimizedSkeletalMeshInstanceDesc& InOutDesc,
	const float InAnimationDeltaTime,
	FOptimizedSkeletalMeshAnimationStats& OutStats)
{
	if (InAnimationDeltaTime <= 0.0f || !InOutDesc.Animation)
	{
		return false;
	}

	const float sequenceLength = InOutDesc.Animation->GetPlayLength();
	if (sequenceLength <= 0.0f)
	{
		return false;
	}

	const float previousTime = InOutDesc.AnimationTime;
	const float advancedTime = previousTime + InAnimationDeltaTime * InOutDesc.AnimationPlayRate;

	if (InOutDesc.bLoopAnimation)
	{
		InOutDesc.AnimationTime = WrapAnimationTime(advancedTime, sequenceLength);
	}
	else
	{
		InOutDesc.AnimationTime = FMath::Clamp(advancedTime, 0.0f, sequenceLength);
		if (InOutDesc.AnimationTime <= 0.0f || InOutDesc.AnimationTime >= sequenceLength)
		{
			InOutDesc.bPlayAnimation = false;
			ActiveAnimationInstanceIds.Remove(InInstanceId);
			++OutStats.FinishedInstances;
		}
	}

	const bool bTimeChanged = !FMath::IsNearlyEqual(previousTime, InOutDesc.AnimationTime);
	if (bTimeChanged)
	{
		++OutStats.AdvancedInstances;
	}

	return bTimeChanged;
}

void UOptimizedSkeletalMeshWorldSubsystem::RemoveInstanceAnimationData(const int32 InInstanceId)
{
	if (InstanceBonePalettes.Remove(InInstanceId) > 0)
	{
		MarkBonePaletteDirty(InInstanceId);
	}

	RemoveAnimationTracking(InInstanceId);
}

void UOptimizedSkeletalMeshWorldSubsystem::BuildAnimationEvaluationWork(
	const float InDeltaTime,
	const TConstArrayView<int32> InInstanceIdsToProcess,
	FOptimizedSkeletalMeshAnimationStats& OutStats,
	TArray<FOptimizedSkeletalMeshAnimationEvaluationWork>& OutEvaluationWork)
{
	OutEvaluationWork.Reset();
	OutEvaluationWork.Reserve(InInstanceIdsToProcess.Num());

	OptimizedSkeletalMesh::FTickAnimationPolicyContext policyContext;
	policyContext.bPauseCpuPoseWhenNotVisible =
		OptimizedSkeletalMesh::CVarAnimationPauseCpuPoseWhenNotVisible.GetValueOnGameThread() != 0;
	policyContext.InvisiblePoseTickRateHz = FMath::Max(
		0.0f,
		OptimizedSkeletalMesh::CVarAnimationInvisiblePoseTickRateHz.GetValueOnGameThread());
	if (const UOptimizedSkeletalMeshSettings* settings = GetDefault<UOptimizedSkeletalMeshSettings>())
	{
		policyContext.bUsesDistanceRateScaling =
			settings->DistanceBasedRateMode != EOptimizedSkeletalMeshDistanceBasedRateMode::Static;
	}

	for (const int32 instanceId : InInstanceIdsToProcess)
	{
		FOptimizedSkeletalMeshInstanceDesc* desc = Instances.Find(instanceId);
		if (!desc)
		{
			RemoveAnimationTracking(instanceId);
			continue;
		}

		if (!desc->Animation || !desc->SkeletalMesh)
		{
			RemoveInstanceAnimationData(instanceId);
			continue;
		}

		++OutStats.AnimatedInstances;
		bool bShouldEvaluate = DirtyAnimationInstanceIds.Contains(instanceId);

		if (ActiveAnimationInstanceIds.Contains(instanceId))
		{
			float animationDeltaTime = InDeltaTime;
			const bool bRenderVisible = RenderVisibleInstanceIds.Contains(instanceId);
			if (OptimizedSkeletalMesh::ShouldUseInvisiblePosePath(policyContext, bRenderVisible))
			{
				if (policyContext.InvisiblePoseTickRateHz > 0.0f)
				{
					const float updateInterval = 1.0f / policyContext.InvisiblePoseTickRateHz;
					float& accumulator = AnimationUpdateAccumulators.FindOrAdd(instanceId);
					accumulator += InDeltaTime;
					if (accumulator < updateInterval)
					{
						++OutStats.SkippedUpdateRateInstances;
						continue;
					}

					animationDeltaTime = accumulator;
					accumulator = 0.0f;
				}
				else
				{
					AnimationUpdateAccumulators.Remove(instanceId);
				}

				AdvanceAnimationTime(instanceId, *desc, animationDeltaTime, OutStats);

				continue;
			}

			float nearestCameraDistance = -1.0f;
			const bool bHasCameraDistance = GetNearestCameraDistance(desc->WorldTransform.GetLocation(), nearestCameraDistance);
			const float effectiveUpdateRateHz = GetEffectiveAnimationUpdateRateHz(
				*desc,
				bHasCameraDistance ? nearestCameraDistance : -1.0f);
			if (OptimizedSkeletalMesh::ShouldCollectDistanceRateStats(policyContext, bHasCameraDistance, effectiveUpdateRateHz))
			{
				++OutStats.DistanceRateScaledInstances;
				if (OutStats.MinEffectiveUpdateRateHz <= 0.0f)
				{
					OutStats.MinEffectiveUpdateRateHz = effectiveUpdateRateHz;
				}
				else
				{
					OutStats.MinEffectiveUpdateRateHz = FMath::Min(OutStats.MinEffectiveUpdateRateHz, effectiveUpdateRateHz);
				}

				OutStats.MaxEffectiveUpdateRateHz = FMath::Max(OutStats.MaxEffectiveUpdateRateHz, effectiveUpdateRateHz);
				OutStats.AverageEffectiveUpdateRateHz += effectiveUpdateRateHz;
			}

			if (effectiveUpdateRateHz > 0.0f)
			{
				const float updateInterval = 1.0f / effectiveUpdateRateHz;
				float& accumulator = AnimationUpdateAccumulators.FindOrAdd(instanceId);
				accumulator += InDeltaTime;
				if (accumulator < updateInterval && !bShouldEvaluate)
				{
					InstanceAnimationBlendAlphas.FindOrAdd(instanceId) = FMath::Clamp(accumulator / updateInterval, 0.0f, 1.0f);
					MarkBonePaletteDirty(instanceId);
					++OutStats.SkippedUpdateRateInstances;
					continue;
				}

				animationDeltaTime = accumulator;
				accumulator = 0.0f;
			}
			else
			{
				AnimationUpdateAccumulators.Remove(instanceId);
			}

			if (AdvanceAnimationTime(instanceId, *desc, animationDeltaTime, OutStats))
			{
				bShouldEvaluate = true;
			}
		}

		if (!bShouldEvaluate)
		{
			continue;
		}

		FOptimizedSkeletalMeshAnimationMeshCache* meshCache = FindOrBuildAnimationMeshCache(desc->SkeletalMesh);
		if (!meshCache)
		{
			if (InstanceBonePalettes.Remove(instanceId) > 0)
			{
				MarkBonePaletteDirty(instanceId);
			}
			++OutStats.FailedPoseEvaluations;
			continue;
		}

		FOptimizedSkeletalMeshAnimationEvaluationWork& work = OutEvaluationWork.AddDefaulted_GetRef();
		work.InstanceId = instanceId;
		work.Desc = *desc;
		work.MeshCache = meshCache;
	}
}

void UOptimizedSkeletalMeshWorldSubsystem::RunAnimationEvaluationWork(
	const TArray<FOptimizedSkeletalMeshAnimationEvaluationWork>& InEvaluationWork,
	FOptimizedSkeletalMeshAnimationStats& OutStats,
	TArray<FOptimizedSkeletalMeshAnimationEvaluationResult>& OutEvaluationResults) const
{
	OutEvaluationResults.SetNum(InEvaluationWork.Num());

	constexpr int32 ParallelEvaluationThreshold = 32;
	if (InEvaluationWork.Num() >= ParallelEvaluationThreshold)
	{
		++OutStats.ParallelPoseBatches;
		ParallelFor(
			InEvaluationWork.Num(),
			[&InEvaluationWork, &OutEvaluationResults](const int32 InWorkIndex) {
				const FOptimizedSkeletalMeshAnimationEvaluationWork& work = InEvaluationWork[InWorkIndex];
				FOptimizedSkeletalMeshAnimationEvaluationResult& result = OutEvaluationResults[InWorkIndex];
				result.InstanceId = work.InstanceId;
				result.bSucceeded =
					work.MeshCache
					&& UOptimizedSkeletalMeshWorldSubsystem::EvaluateInstanceBonePaletteWithCache(
						work.Desc,
						*work.MeshCache,
						result.BonePalette);
			});
	}
	else
	{
		for (int32 workIndex = 0; workIndex < InEvaluationWork.Num(); ++workIndex)
		{
			const FOptimizedSkeletalMeshAnimationEvaluationWork& work = InEvaluationWork[workIndex];
			FOptimizedSkeletalMeshAnimationEvaluationResult& result = OutEvaluationResults[workIndex];
			result.InstanceId = work.InstanceId;
			result.bSucceeded =
				work.MeshCache
				&& EvaluateInstanceBonePaletteWithCache(
					work.Desc,
					*work.MeshCache,
					result.BonePalette);
		}
	}
}

void UOptimizedSkeletalMeshWorldSubsystem::ApplyAnimationEvaluationResults(
	TArray<FOptimizedSkeletalMeshAnimationEvaluationResult>& InOutEvaluationResults,
	FOptimizedSkeletalMeshAnimationStats& OutStats)
{
	for (FOptimizedSkeletalMeshAnimationEvaluationResult& result : InOutEvaluationResults)
	{
		if (result.bSucceeded)
		{
			++OutStats.PoseEvaluatedInstances;
			if (const TArray<FMatrix44f>* currentBonePalette = InstanceBonePalettes.Find(result.InstanceId))
			{
				PreviousInstanceBonePalettes.FindOrAdd(result.InstanceId) = *currentBonePalette;
			}
			else
			{
				PreviousInstanceBonePalettes.FindOrAdd(result.InstanceId) = result.BonePalette;
			}

			TArray<FMatrix44f>& bonePalette = InstanceBonePalettes.FindOrAdd(result.InstanceId);
			bonePalette = MoveTemp(result.BonePalette);
			InstanceAnimationBlendAlphas.FindOrAdd(result.InstanceId) = 0.0f;
			MarkBonePaletteDirty(result.InstanceId);
		}
		else
		{
			PreviousInstanceBonePalettes.Remove(result.InstanceId);
			if (InstanceBonePalettes.Remove(result.InstanceId) > 0)
			{
				MarkBonePaletteDirty(result.InstanceId);
			}
			InstanceAnimationBlendAlphas.Remove(result.InstanceId);
			++OutStats.FailedPoseEvaluations;
		}
	}
}

void UOptimizedSkeletalMeshWorldSubsystem::ClearProcessedDirtyAnimationIds(
	const TConstArrayView<int32> InProcessedDirtyInstanceIds)
{
	for (const int32 instanceId : InProcessedDirtyInstanceIds)
	{
		DirtyAnimationInstanceIds.Remove(instanceId);
	}
}

FOptimizedSkeletalMeshAnimationMeshCache* UOptimizedSkeletalMeshWorldSubsystem::FindOrBuildAnimationMeshCache(
	USkeletalMesh* InSkeletalMesh)
{
	if (!InSkeletalMesh)
	{
		return nullptr;
	}

	const TObjectKey<USkeletalMesh> meshKey(InSkeletalMesh);
	if (FOptimizedSkeletalMeshAnimationMeshCache* existingCache = AnimationMeshCaches.Find(meshKey))
	{
		return existingCache;
	}

	const FReferenceSkeleton& refSkeleton = InSkeletalMesh->GetRefSkeleton();
	const int32 boneCount = refSkeleton.GetNum();
	if (boneCount <= 0)
	{
		return nullptr;
	}

	FOptimizedSkeletalMeshAnimationMeshCache& newCache = AnimationMeshCaches.Add(meshKey);
	newCache.RequiredBoneIndices.Reset(boneCount);
	newCache.RequiredBoneIndices.Reserve(boneCount);
	for (int32 boneIndex = 0; boneIndex < boneCount; ++boneIndex)
	{
		newCache.RequiredBoneIndices.Add(static_cast<FBoneIndexType>(boneIndex));
	}

	newCache.RequiredBones.InitializeTo(
		newCache.RequiredBoneIndices,
		UE::Anim::FCurveFilterSettings(UE::Anim::ECurveFilterMode::DisallowAll),
		*InSkeletalMesh);

	TArray<FTransform> refComponentSpaceTransforms;
	refComponentSpaceTransforms.SetNum(boneCount);
	const TArray<FTransform>& refLocalTransforms = refSkeleton.GetRefBonePose();
	for (int32 boneIndex = 0; boneIndex < boneCount; ++boneIndex)
	{
		const int32 parentIndex = refSkeleton.GetParentIndex(boneIndex);
		if (parentIndex != INDEX_NONE)
		{
			refComponentSpaceTransforms[boneIndex] = refLocalTransforms[boneIndex] * refComponentSpaceTransforms[parentIndex];
		}
		else
		{
			refComponentSpaceTransforms[boneIndex] = refLocalTransforms[boneIndex];
		}
	}

	newCache.RefBasesInvMatrices.SetNum(boneCount);
	for (int32 boneIndex = 0; boneIndex < boneCount; ++boneIndex)
	{
		newCache.RefBasesInvMatrices[boneIndex] = FMatrix44f(refComponentSpaceTransforms[boneIndex].ToMatrixWithScale().Inverse());
	}

	return &newCache;
}

bool UOptimizedSkeletalMeshWorldSubsystem::EvaluateInstanceBonePalette(
	const FOptimizedSkeletalMeshInstanceDesc& InDesc,
	TArray<FMatrix44f>& OutBonePalette)
{
	if (!InDesc.SkeletalMesh || !InDesc.Animation)
	{
		OutBonePalette.Reset();
		return false;
	}

	FOptimizedSkeletalMeshAnimationMeshCache* meshCache = FindOrBuildAnimationMeshCache(InDesc.SkeletalMesh);
	if (!meshCache || meshCache->RequiredBoneIndices.IsEmpty())
	{
		OutBonePalette.Reset();
		return false;
	}

	return EvaluateInstanceBonePaletteWithCache(InDesc, *meshCache, OutBonePalette);
}

bool UOptimizedSkeletalMeshWorldSubsystem::EvaluateInstanceBonePaletteWithCache(
	const FOptimizedSkeletalMeshInstanceDesc& InDesc,
	const FOptimizedSkeletalMeshAnimationMeshCache& InMeshCache,
	TArray<FMatrix44f>& OutBonePalette)
{
	if (!InDesc.SkeletalMesh || !InDesc.Animation || InMeshCache.RequiredBoneIndices.IsEmpty())
	{
		OutBonePalette.Reset();
		return false;
	}

	FCompactPose localPose;
	localPose.ResetToRefPose(InMeshCache.RequiredBones);

	FBlendedCurve curve;
	curve.InitFrom(InMeshCache.RequiredBones);

	UE::Anim::FStackAttributeContainer attributes;
	FAnimationPoseData animationPoseData(localPose, curve, attributes);

	const FAnimExtractContext extractionContext(static_cast<double>(InDesc.AnimationTime), false, FDeltaTimeRecord(0.0f), InDesc.bLoopAnimation);
	InDesc.Animation->GetBonePose(animationPoseData, extractionContext);

	const int32 boneCount = InMeshCache.RequiredBones.GetNumBones();
	TArray<FTransform> componentSpaceTransforms;
	componentSpaceTransforms.SetNum(boneCount);

	for (const FCompactPoseBoneIndex compactBoneIndex : localPose.ForEachBoneIndex())
	{
		const FMeshPoseBoneIndex meshBoneIndex = InMeshCache.RequiredBones.MakeMeshPoseIndex(compactBoneIndex);
		const int32 boneIndex = meshBoneIndex.GetInt();
		if (!componentSpaceTransforms.IsValidIndex(boneIndex))
		{
			continue;
		}

		const FCompactPoseBoneIndex parentCompactBoneIndex = localPose.GetParentBoneIndex(compactBoneIndex);
		if (parentCompactBoneIndex != INDEX_NONE)
		{
			const FMeshPoseBoneIndex parentMeshBoneIndex = InMeshCache.RequiredBones.MakeMeshPoseIndex(parentCompactBoneIndex);
			componentSpaceTransforms[boneIndex] = localPose[compactBoneIndex] * componentSpaceTransforms[parentMeshBoneIndex.GetInt()];
		}
		else
		{
			componentSpaceTransforms[boneIndex] = localPose[compactBoneIndex];
		}
	}

	OutBonePalette.SetNum(boneCount);
	for (int32 boneIndex = 0; boneIndex < boneCount; ++boneIndex)
	{
		const FMatrix44f componentSpaceMatrix(componentSpaceTransforms[boneIndex].ToMatrixWithScale());
		OutBonePalette[boneIndex] = InMeshCache.RefBasesInvMatrices[boneIndex] * componentSpaceMatrix;
	}

	return true;
}

float UOptimizedSkeletalMeshWorldSubsystem::WrapAnimationTime(
	const float InAnimationTime,
	const float InSequenceLength)
{
	if (InSequenceLength <= 0.0f)
	{
		return 0.0f;
	}

	float wrappedTime = FMath::Fmod(InAnimationTime, InSequenceLength);
	if (wrappedTime < 0.0f)
	{
		wrappedTime += InSequenceLength;
	}

	return wrappedTime;
}

void UOptimizedSkeletalMeshWorldSubsystem::DrawInstanceDebugOverlay() const
{
	if (!OptimizedSkeletalMesh::bDebugEntitiesEnabled)
	{
		return;
	}

	UWorld* world = GetWorld();
	if (!world || !world->IsGameWorld())
	{
		return;
	}

	FVector cameraLocation = FVector::ZeroVector;
	FRotator cameraRotation = FRotator::ZeroRotator;
	bool bHasCamera = false;
	for (FConstPlayerControllerIterator controllerIterator = world->GetPlayerControllerIterator(); controllerIterator; ++controllerIterator)
	{
		const APlayerController* playerController = controllerIterator->Get();
		if (!playerController)
		{
			continue;
		}

		playerController->GetPlayerViewPoint(cameraLocation, cameraRotation);
		bHasCamera = true;
		break;
	}

	if (!bHasCamera)
	{
		return;
	}

	const float radius = FMath::Max(100.0f, OptimizedSkeletalMesh::DebugEntitiesRadius);
	const float radiusSquared = radius * radius;
	const int32 maxCount = FMath::Max(1, OptimizedSkeletalMesh::DebugEntitiesMaxCount);

	struct FDebugEntry
	{
		int32 instanceId = INDEX_NONE;
		int32 batchIndex = INDEX_NONE;
		int32 lodIndex = 0;
		float distance = 0.0f;
		FVector worldLocation = FVector::ZeroVector;
		const FOptimizedSkeletalMeshInstanceDesc* desc = nullptr;
	};

	TArray<FDebugEntry> entries;
	entries.Reserve(Instances.Num());

	TMap<USkeletalMesh*, int32> batchByMesh;
	int32 nextBatchIndex = 0;

	for (const TPair<int32, FOptimizedSkeletalMeshInstanceDesc>& pair : Instances)
	{
		const FOptimizedSkeletalMeshInstanceDesc& desc = pair.Value;
		if (!desc.SkeletalMesh || !desc.bVisible)
		{
			continue;
		}

		const FVector worldLocation = desc.WorldTransform.GetLocation();
		const float distanceSquared = FVector::DistSquared(worldLocation, cameraLocation);
		if (distanceSquared > radiusSquared)
		{
			continue;
		}

		int32 batchIndex = INDEX_NONE;
		if (int32* existingBatchIndex = batchByMesh.Find(desc.SkeletalMesh.Get()))
		{
			batchIndex = *existingBatchIndex;
		}
		else
		{
			batchIndex = nextBatchIndex++;
			batchByMesh.Add(desc.SkeletalMesh.Get(), batchIndex);
		}

		int32 lodIndex = FMath::Max(0, desc.LODIndex);
		if (desc.bAutoLOD)
		{
			lodIndex = 0;
			if (const FSkeletalMeshRenderData* renderData = desc.SkeletalMesh->GetResourceForRendering())
			{
				const int32 lodCount = renderData->LODRenderData.Num();
				if (lodCount > 1)
				{
					const FBoxSphereBounds bounds(desc.SkeletalMesh->GetBounds().TransformBy(desc.WorldTransform));
					const FVector cameraForward = cameraRotation.Vector();
					const FVector toCenter = bounds.Origin - cameraLocation;
					const float projectedDistance = FMath::Max(FVector::DotProduct(toCenter, cameraForward), 1.0f);
					const float screenRadius = bounds.SphereRadius / projectedDistance;
					float screenSize = screenRadius * 2.0f;

					for (int32 candidateLod = 1; candidateLod < lodCount; ++candidateLod)
					{
						if (const FSkeletalMeshLODInfo* lodInfo = desc.SkeletalMesh->GetLODInfo(candidateLod))
						{
							if (screenSize <= lodInfo->ScreenSize.Default)
							{
								lodIndex = candidateLod;
							}
						}
					}
				}
			}
		}

		FDebugEntry& entry = entries.AddDefaulted_GetRef();
		entry.instanceId = pair.Key;
		entry.batchIndex = batchIndex;
		entry.lodIndex = lodIndex;
		entry.distance = FMath::Sqrt(distanceSquared);
		entry.worldLocation = worldLocation;
		entry.desc = &desc;
	}

	entries.Sort([](const FDebugEntry& InA, const FDebugEntry& InB) {
		return InA.distance < InB.distance;
	});

	if (entries.Num() > maxCount)
	{
		entries.SetNum(maxCount, EAllowShrinking::No);
	}

	for (const FDebugEntry& entry : entries)
	{
		if (!entry.desc)
		{
			continue;
		}

		const FString line = FString::Printf(
			TEXT("id:%d b:%d lod:%d dist:%.0f\nmesh:%s\nanim:%s t:%.2f play:%d loop:%d"),
			entry.instanceId,
			entry.batchIndex,
			entry.lodIndex,
			entry.distance,
			*GetNameSafe(entry.desc->SkeletalMesh),
			*GetNameSafe(entry.desc->Animation),
			entry.desc->AnimationTime,
			entry.desc->bPlayAnimation ? 1 : 0,
			entry.desc->bLoopAnimation ? 1 : 0);

		static const FColor lodColors[] = {
			FColor(255, 255, 255),
			FColor(120, 220, 120),
			FColor(120, 180, 255),
			FColor(255, 220, 120),
			FColor(255, 160, 120),
			FColor(220, 120, 255),
			FColor(200, 200, 120),
			FColor(180, 180, 180)};
		const int32 colorIndex = FMath::Clamp(entry.lodIndex, 0, static_cast<int32>(UE_ARRAY_COUNT(lodColors)) - 1);
		const FColor textColor = lodColors[colorIndex];

		DrawDebugString(
			world,
			entry.worldLocation + FVector(0.0f, 0.0f, 120.0f),
			line,
			nullptr,
			textColor,
			0.0f,
			true);
	}
}
