// Copyright Epic Games, Inc. All Rights Reserved.

#include "Runtime/Manager/OptimizedSkeletalMeshWorldSubsystem.h"

#include "Async/ParallelFor.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"
#include "BonePose.h"
#include "HAL/IConsoleManager.h"
#include "HAL/ThreadSafeCounter.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
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
		}

		settings.InstanceCullBoundsScale = FMath::Max(1.0f, settings.InstanceCullBoundsScale);
		settings.ConservativeProxyBoundsExtent = FMath::Max(1000.0f, settings.ConservativeProxyBoundsExtent);
		settings.MaxShadowCastDistance = FMath::Max(0.0f, settings.MaxShadowCastDistance);
		settings.NearFullShadowDistance = FMath::Max(0.0f, settings.NearFullShadowDistance);
		settings.MidShadowDistance = FMath::Max(settings.NearFullShadowDistance, settings.MidShadowDistance);
		settings.MidShadowUpdateDivisor = FMath::Max(1, settings.MidShadowUpdateDivisor);
		settings.FarShadowUpdateDivisor = FMath::Max(0, settings.FarShadowUpdateDivisor);
		settings.MaxDynamicShadowCasters = FMath::Max(0, settings.MaxDynamicShadowCasters);
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

	static TAutoConsoleVariable<int32> CVarAnimationMaxDirtyEvaluationsPerFrame(
		TEXT("osm.Animation.MaxDirtyEvaluationsPerFrame"),
		512,
		TEXT("Maximum number of dirty animation instances to evaluate per frame.\n")
		TEXT("<= 0 means no limit."),
		ECVF_Default);

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
			&& InLeft.MaxDynamicShadowCasters == InRight.MaxDynamicShadowCasters;
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

	struct FAnimationEvaluationWork
	{
		int32 InstanceId = INDEX_NONE;
		FOptimizedSkeletalMeshInstanceDesc Desc;
		const FOptimizedSkeletalMeshAnimationMeshCache* MeshCache = nullptr;
	};

	struct FAnimationEvaluationResult
	{
		int32 InstanceId = INDEX_NONE;
		TArray<FMatrix44f> BonePalette;
		bool bSucceeded = false;
	};
} // namespace OptimizedSkeletalMesh

void UOptimizedSkeletalMeshWorldSubsystem::Initialize(FSubsystemCollectionBase& InCollection)
{
	Super::Initialize(InCollection);
	OptimizedSkeletalMesh::SetSettingsBackSyncEnabled(true);

	Instances.Reset();
	FreeInstanceIds.Reset();
	AnimationMeshCaches.Reset();
	InstanceBonePalettes.Reset();
	ActiveAnimationInstanceIds.Reset();
	DirtyAnimationInstanceIds.Reset();
	DirtyBonePaletteInstanceIds.Reset();
	RenderVisibleInstanceIds.Reset();
	AnimationUpdateAccumulators.Reset();
	ExternalRenderComponents.Reset();
	NextInstanceId = 1;
	LastSeenRenderCVarVersion = OptimizedSkeletalMesh::GetRenderCVarChangeVersion();
	RenderStateRecoveryAttempts = 0;
	bRenderDataDirty = false;
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
	InstanceBonePalettes.Reset();
	ActiveAnimationInstanceIds.Reset();
	DirtyAnimationInstanceIds.Reset();
	DirtyBonePaletteInstanceIds.Reset();
	RenderVisibleInstanceIds.Reset();
	AnimationUpdateAccumulators.Reset();
	ExternalRenderComponents.Reset();
	NextInstanceId = 1;
	LastSeenRenderCVarVersion = OptimizedSkeletalMesh::GetRenderCVarChangeVersion();
	RenderStateRecoveryAttempts = 0;
	bRenderDataDirty = false;
	LastAnimationStats = FOptimizedSkeletalMeshAnimationStats();
	LastRenderStats = FOptimizedSkeletalMeshRenderStats();
	CurrentRenderSettings = OptimizedSkeletalMesh::BuildRenderSettingsFromProjectSettings();
	ActiveRenderSettings = CurrentRenderSettings;
	OptimizedSkeletalMesh::PublishAnimationStats(LastAnimationStats);

	Super::Deinitialize();
}

void UOptimizedSkeletalMeshWorldSubsystem::Tick(float InDeltaTime)
{
	static bool bLoggedTickProbe = false;
	if (!bLoggedTickProbe)
	{
		bLoggedTickProbe = true;
		const UWorld* world = GetWorld();
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("OSM Subsystem TickProbe: world=%s type=%d netmode=%d instances=%d renderComp=%s registered=%d renderState=%d"),
			*GetNameSafe(world),
			world ? static_cast<int32>(world->WorldType) : -1,
			world ? static_cast<int32>(world->GetNetMode()) : -1,
			Instances.Num(),
			RenderComponent ? TEXT("yes") : TEXT("no"),
			(RenderComponent && RenderComponent->IsRegistered()) ? 1 : 0,
			(RenderComponent && RenderComponent->IsRenderStateCreated()) ? 1 : 0);
	}

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
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("OSM RenderState recovery: attempt=%d world=%s type=%d netmode=%d"),
				RenderStateRecoveryAttempts,
				*GetNameSafe(world),
				static_cast<int32>(world->WorldType),
				static_cast<int32>(world->GetNetMode()));
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
	if (RenderComponent && HasDirtyRenderVisibleBonePalettes() && RenderComponent->PushBonePalettesToRenderThread())
	{
		ClearDirtyRenderVisibleBonePalettes();
	}

	if (bRenderDataDirty)
	{
		if (RenderComponent)
		{
			RenderComponent->RequestRenderRefresh();
		}

		ClearRenderDataDirty();
	}
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
	InstanceBonePalettes.Remove(InHandle.Id);
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

	instance->WorldTransform = InWorldTransform;
	MarkRenderDataDirty();

	return true;
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

	OutInstances.Sort(
		[](const FOptimizedSkeletalMeshInstanceSnapshot& InLeft, const FOptimizedSkeletalMeshInstanceSnapshot& InRight) {
			return InLeft.Handle.Id < InRight.Handle.Id;
		});
}

int32 UOptimizedSkeletalMeshWorldSubsystem::GetInstanceCount() const
{
	return Instances.Num();
}

int32 UOptimizedSkeletalMeshWorldSubsystem::GetVisibleRenderBatchCount() const
{
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

	return visibleMeshes.Num();
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
}

const TArray<FMatrix44f>* UOptimizedSkeletalMeshWorldSubsystem::GetInstanceBonePalette(
	const FOptimizedSkeletalMeshInstanceHandle InHandle) const
{
	return InstanceBonePalettes.Find(InHandle.Id);
}

void UOptimizedSkeletalMeshWorldSubsystem::GetBonePaletteSnapshots(
	TArray<FOptimizedSkeletalMeshBonePaletteSnapshot>& OutSnapshots) const
{
	OutSnapshots.Reset(RenderVisibleInstanceIds.Num());

	for (const int32 instanceId : RenderVisibleInstanceIds)
	{
		const TArray<FMatrix44f>* bonePalette = InstanceBonePalettes.Find(instanceId);
		if (!bonePalette || bonePalette->IsEmpty())
		{
			continue;
		}

		FOptimizedSkeletalMeshBonePaletteSnapshot& snapshot = OutSnapshots.AddDefaulted_GetRef();
		snapshot.Handle = FOptimizedSkeletalMeshInstanceHandle(instanceId);
		snapshot.BonePalette = *bonePalette;
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
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("OSM EnsureRenderBridge: using bridge actor component, registered=%d renderState=%d world=%s type=%d netmode=%d"),
				RenderComponent->IsRegistered() ? 1 : 0,
				RenderComponent->IsRenderStateCreated() ? 1 : 0,
				*GetNameSafe(world),
				static_cast<int32>(world->WorldType),
				static_cast<int32>(world->GetNetMode()));
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
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("OSM EnsureRenderBridge: re-registered component, registered=%d renderState=%d world=%s type=%d netmode=%d"),
			RenderComponent->IsRegistered() ? 1 : 0,
			RenderComponent->IsRenderStateCreated() ? 1 : 0,
			*GetNameSafe(world),
			static_cast<int32>(world->WorldType),
			static_cast<int32>(world->GetNetMode()));
	}
}

void UOptimizedSkeletalMeshWorldSubsystem::DestroyRenderBridge()
{
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

	if (RenderComponent && BulkUpdateDepth <= 0)
	{
		RenderComponent->RequestRenderRefresh();
	}
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
	RenderVisibleInstanceIds.Remove(InInstanceId);
	AnimationUpdateAccumulators.Remove(InInstanceId);
}

void UOptimizedSkeletalMeshWorldSubsystem::MarkBonePaletteDirty(const int32 InInstanceId)
{
	DirtyBonePaletteInstanceIds.Add(InInstanceId);
}

bool UOptimizedSkeletalMeshWorldSubsystem::HasDirtyRenderVisibleBonePalettes() const
{
	for (const int32 instanceId : DirtyBonePaletteInstanceIds)
	{
		if (RenderVisibleInstanceIds.Contains(instanceId))
		{
			return true;
		}
	}

	return false;
}

void UOptimizedSkeletalMeshWorldSubsystem::ClearDirtyRenderVisibleBonePalettes()
{
	for (auto iterator = DirtyBonePaletteInstanceIds.CreateIterator(); iterator; ++iterator)
	{
		if (RenderVisibleInstanceIds.Contains(*iterator))
		{
			iterator.RemoveCurrent();
		}
	}
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
	newStats.RegisteredInstances = Instances.Num();
	newStats.LastDeltaTime = InDeltaTime;
	newStats.ActiveAnimationInstances = ActiveAnimationInstanceIds.Num();
	newStats.DirtyAnimationInstances = DirtyAnimationInstanceIds.Num();
	newStats.RenderVisibleAnimatedInstances = RenderVisibleInstanceIds.Num();
	newStats.DirtyCpuPaletteInstances = DirtyBonePaletteInstanceIds.Num();
	newStats.DirtyGpuPaletteInstances = 0;
	for (const int32 instanceId : DirtyBonePaletteInstanceIds)
	{
		if (RenderVisibleInstanceIds.Contains(instanceId))
		{
			++newStats.DirtyGpuPaletteInstances;
		}
		else
		{
			++newStats.GpuPaletteUploadSkippedInstances;
		}
	}

	if (ActiveAnimationInstanceIds.IsEmpty() && DirtyAnimationInstanceIds.IsEmpty())
	{
		newStats.BonePaletteInstances = InstanceBonePalettes.Num();
		for (const TPair<int32, TArray<FMatrix44f>>& pair : InstanceBonePalettes)
		{
			newStats.TotalBoneMatrices += pair.Value.Num();
			newStats.MaxBonesPerInstance = FMath::Max(newStats.MaxBonesPerInstance, pair.Value.Num());
		}

		newStats.DirtyCpuPaletteInstances = DirtyBonePaletteInstanceIds.Num();
		newStats.DirtyGpuPaletteInstances = 0;
		newStats.GpuPaletteUploadSkippedInstances = 0;
		for (const int32 instanceId : DirtyBonePaletteInstanceIds)
		{
			if (RenderVisibleInstanceIds.Contains(instanceId))
			{
				++newStats.DirtyGpuPaletteInstances;
			}
			else
			{
				++newStats.GpuPaletteUploadSkippedInstances;
			}
		}

		LastAnimationStats = newStats;
		OptimizedSkeletalMesh::PublishAnimationStats(LastAnimationStats);
		return;
	}

	TArray<int32> instanceIdsToProcess;
	instanceIdsToProcess.Reserve(ActiveAnimationInstanceIds.Num() + DirtyAnimationInstanceIds.Num());

	TArray<int32> dirtyInstanceIdsToProcess;
	const int32 maxDirtyEvaluationsPerFrame = OptimizedSkeletalMesh::CVarAnimationMaxDirtyEvaluationsPerFrame.GetValueOnGameThread();
	const int32 maxDirtyCount = maxDirtyEvaluationsPerFrame > 0
		? maxDirtyEvaluationsPerFrame
		: TNumericLimits<int32>::Max();

	dirtyInstanceIdsToProcess.Reserve(FMath::Min(DirtyAnimationInstanceIds.Num(), maxDirtyCount));
	for (const int32 instanceId : DirtyAnimationInstanceIds)
	{
		dirtyInstanceIdsToProcess.Add(instanceId);
		if (dirtyInstanceIdsToProcess.Num() >= maxDirtyCount)
		{
			break;
		}
	}

	for (const int32 instanceId : dirtyInstanceIdsToProcess)
	{
		instanceIdsToProcess.AddUnique(instanceId);
	}

	for (const int32 instanceId : ActiveAnimationInstanceIds)
	{
		instanceIdsToProcess.AddUnique(instanceId);
	}

	TArray<OptimizedSkeletalMesh::FAnimationEvaluationWork> evaluationWork;
	evaluationWork.Reserve(instanceIdsToProcess.Num());

	for (const int32 instanceId : instanceIdsToProcess)
	{
		FOptimizedSkeletalMeshInstanceDesc* desc = Instances.Find(instanceId);
		if (!desc)
		{
			RemoveAnimationTracking(instanceId);
			continue;
		}

		if (!desc->Animation || !desc->SkeletalMesh)
		{
			if (InstanceBonePalettes.Remove(instanceId) > 0)
			{
				MarkBonePaletteDirty(instanceId);
			}
			RemoveAnimationTracking(instanceId);
			continue;
		}

		++newStats.AnimatedInstances;
		bool bShouldEvaluate = DirtyAnimationInstanceIds.Contains(instanceId);

		if (ActiveAnimationInstanceIds.Contains(instanceId))
		{
			float animationDeltaTime = InDeltaTime;
			float nearestCameraDistance = -1.0f;
			const bool bHasCameraDistance = GetNearestCameraDistance(desc->WorldTransform.GetLocation(), nearestCameraDistance);
			const float effectiveUpdateRateHz = GetEffectiveAnimationUpdateRateHz(
				*desc,
				bHasCameraDistance ? nearestCameraDistance : -1.0f);
			const UOptimizedSkeletalMeshSettings* settings = GetDefault<UOptimizedSkeletalMeshSettings>();
			const bool bUsesDistanceRateScaling =
				settings
				&& settings->DistanceBasedRateMode != EOptimizedSkeletalMeshDistanceBasedRateMode::Static;
			if (bUsesDistanceRateScaling && bHasCameraDistance && effectiveUpdateRateHz > 0.0f)
			{
				++newStats.DistanceRateScaledInstances;
				if (newStats.MinEffectiveUpdateRateHz <= 0.0f)
				{
					newStats.MinEffectiveUpdateRateHz = effectiveUpdateRateHz;
				}
				else
				{
					newStats.MinEffectiveUpdateRateHz = FMath::Min(newStats.MinEffectiveUpdateRateHz, effectiveUpdateRateHz);
				}

				newStats.MaxEffectiveUpdateRateHz = FMath::Max(newStats.MaxEffectiveUpdateRateHz, effectiveUpdateRateHz);
				newStats.AverageEffectiveUpdateRateHz += effectiveUpdateRateHz;
			}

			if (effectiveUpdateRateHz > 0.0f)
			{
				const float updateInterval = 1.0f / effectiveUpdateRateHz;
				float& accumulator = AnimationUpdateAccumulators.FindOrAdd(instanceId);
				accumulator += InDeltaTime;
				if (accumulator < updateInterval && !bShouldEvaluate)
				{
					++newStats.SkippedUpdateRateInstances;
					continue;
				}

				animationDeltaTime = accumulator;
				accumulator = 0.0f;
			}
			else
			{
				AnimationUpdateAccumulators.Remove(instanceId);
			}

			if (animationDeltaTime > 0.0f)
			{
				const float sequenceLength = desc->Animation->GetPlayLength();
				if (sequenceLength > 0.0f)
				{
					const float previousTime = desc->AnimationTime;
					const float advancedTime = previousTime + animationDeltaTime * desc->AnimationPlayRate;

					if (desc->bLoopAnimation)
					{
						desc->AnimationTime = WrapAnimationTime(advancedTime, sequenceLength);
					}
					else
					{
						desc->AnimationTime = FMath::Clamp(advancedTime, 0.0f, sequenceLength);
						if (desc->AnimationTime <= 0.0f || desc->AnimationTime >= sequenceLength)
						{
							desc->bPlayAnimation = false;
							ActiveAnimationInstanceIds.Remove(instanceId);
							++newStats.FinishedInstances;
						}
					}

					if (!FMath::IsNearlyEqual(previousTime, desc->AnimationTime))
					{
						++newStats.AdvancedInstances;
						bShouldEvaluate = true;
					}
				}
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
			++newStats.FailedPoseEvaluations;
			continue;
		}

		OptimizedSkeletalMesh::FAnimationEvaluationWork& work = evaluationWork.AddDefaulted_GetRef();
		work.InstanceId = instanceId;
		work.Desc = *desc;
		work.MeshCache = meshCache;
	}

	TArray<OptimizedSkeletalMesh::FAnimationEvaluationResult> evaluationResults;
	evaluationResults.SetNum(evaluationWork.Num());

	constexpr int32 ParallelEvaluationThreshold = 32;
	if (evaluationWork.Num() >= ParallelEvaluationThreshold)
	{
		++newStats.ParallelPoseBatches;
		ParallelFor(
			evaluationWork.Num(),
			[&evaluationWork, &evaluationResults](const int32 InWorkIndex) {
				const OptimizedSkeletalMesh::FAnimationEvaluationWork& work = evaluationWork[InWorkIndex];
				OptimizedSkeletalMesh::FAnimationEvaluationResult& result = evaluationResults[InWorkIndex];
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
		for (int32 workIndex = 0; workIndex < evaluationWork.Num(); ++workIndex)
		{
			const OptimizedSkeletalMesh::FAnimationEvaluationWork& work = evaluationWork[workIndex];
			OptimizedSkeletalMesh::FAnimationEvaluationResult& result = evaluationResults[workIndex];
			result.InstanceId = work.InstanceId;
			result.bSucceeded =
				work.MeshCache
				&& EvaluateInstanceBonePaletteWithCache(
					work.Desc,
					*work.MeshCache,
					result.BonePalette);
		}
	}

	if (newStats.DistanceRateScaledInstances > 0)
	{
		newStats.AverageEffectiveUpdateRateHz /= static_cast<float>(newStats.DistanceRateScaledInstances);
	}

	for (OptimizedSkeletalMesh::FAnimationEvaluationResult& result : evaluationResults)
	{
		if (result.bSucceeded)
		{
			++newStats.PoseEvaluatedInstances;
			TArray<FMatrix44f>& bonePalette = InstanceBonePalettes.FindOrAdd(result.InstanceId);
			bonePalette = MoveTemp(result.BonePalette);
			MarkBonePaletteDirty(result.InstanceId);
		}
		else
		{
			if (InstanceBonePalettes.Remove(result.InstanceId) > 0)
			{
				MarkBonePaletteDirty(result.InstanceId);
			}
			++newStats.FailedPoseEvaluations;
		}
	}

	for (const int32 instanceId : dirtyInstanceIdsToProcess)
	{
		DirtyAnimationInstanceIds.Remove(instanceId);
	}

	newStats.BonePaletteInstances = InstanceBonePalettes.Num();
	for (const TPair<int32, TArray<FMatrix44f>>& pair : InstanceBonePalettes)
	{
		newStats.TotalBoneMatrices += pair.Value.Num();
		newStats.MaxBonesPerInstance = FMath::Max(newStats.MaxBonesPerInstance, pair.Value.Num());
	}
	newStats.ActiveAnimationInstances = ActiveAnimationInstanceIds.Num();
	newStats.DirtyAnimationInstances = DirtyAnimationInstanceIds.Num();
	newStats.RenderVisibleAnimatedInstances = RenderVisibleInstanceIds.Num();
	newStats.DirtyCpuPaletteInstances = DirtyBonePaletteInstanceIds.Num();
	newStats.DirtyGpuPaletteInstances = 0;
	newStats.GpuPaletteUploadSkippedInstances = 0;
	for (const int32 instanceId : DirtyBonePaletteInstanceIds)
	{
		if (RenderVisibleInstanceIds.Contains(instanceId))
		{
			++newStats.DirtyGpuPaletteInstances;
		}
		else
		{
			++newStats.GpuPaletteUploadSkippedInstances;
		}
	}

	LastAnimationStats = newStats;
	OptimizedSkeletalMesh::PublishAnimationStats(LastAnimationStats);
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
