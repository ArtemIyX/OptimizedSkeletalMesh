// Copyright Epic Games, Inc. All Rights Reserved.

#include "Runtime/Settings/OptimizedSkeletalMeshSettings.h"

#include "HAL/IConsoleManager.h"

namespace OptimizedSkeletalMesh
{
	static bool bRenderSettingsSyncInProgress = false;

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

	static void PushRenderSettingsToCVars(const UOptimizedSkeletalMeshSettings& InSettings)
	{
		TGuardValue<bool> syncGuard(bRenderSettingsSyncInProgress, true);
		SetConsoleInt(TEXT("osm.Render.DrawDebugBounds"), InSettings.bDrawDebugBounds ? 1 : 0);
		SetConsoleInt(TEXT("osm.Render.DrawMeshSections"), InSettings.bDrawMeshSections ? 1 : 0);
		SetConsoleInt(TEXT("osm.Render.DrawMode"), static_cast<int32>(InSettings.MeshDrawMode));
		SetConsoleInt(TEXT("osm.Render.InstanceFrustumCulling"), InSettings.bEnableInstanceFrustumCulling ? 1 : 0);
		SetConsoleFloat(TEXT("osm.Render.InstanceCullBoundsScale"), InSettings.InstanceCullBoundsScale);
		SetConsoleInt(TEXT("osm.Render.UseConservativeProxyBounds"), InSettings.bUseConservativeProxyBounds ? 1 : 0);
		SetConsoleFloat(TEXT("osm.Render.ConservativeProxyBoundsExtent"), InSettings.ConservativeProxyBoundsExtent);
		SetConsoleInt(TEXT("osm.Render.DrawCullingDebug"), InSettings.bDrawCullingDebug ? 1 : 0);
		SetConsoleInt(TEXT("osm.Render.DrawCullTestBounds"), InSettings.bDrawCullTestBounds ? 1 : 0);
		SetConsoleInt(TEXT("osm.Render.CastShadows"), InSettings.bCastShadows ? 1 : 0);
		SetConsoleFloat(TEXT("osm.Render.NearFullShadowDistance"), InSettings.NearFullShadowDistance);
		SetConsoleFloat(TEXT("osm.Render.MidShadowDistance"), InSettings.MidShadowDistance);
		SetConsoleInt(TEXT("osm.Render.MidShadowUpdateDivisor"), InSettings.MidShadowUpdateDivisor);
		SetConsoleInt(TEXT("osm.Render.FarShadowUpdateDivisor"), InSettings.FarShadowUpdateDivisor);
		SetConsoleFloat(TEXT("osm.Render.MaxShadowCastDistance"), InSettings.MaxShadowCastDistance);
		SetConsoleInt(TEXT("osm.Render.MaxDynamicShadowCasters"), InSettings.MaxDynamicShadowCasters);
		SetConsoleInt(TEXT("osm.Render.NearShadowLodBias"), InSettings.NearShadowLodBias);
		SetConsoleInt(TEXT("osm.Render.MidShadowLodBias"), InSettings.MidShadowLodBias);
		SetConsoleInt(TEXT("osm.Render.FarShadowLodBias"), InSettings.FarShadowLodBias);
		SetConsoleInt(TEXT("osm.Render.MaxShadowSectionsPerLOD"), InSettings.MaxShadowSectionsPerLOD);
		SetConsoleFloat(TEXT("osm.Render.LocalLightMaxShadowCastDistance"), InSettings.LocalLightMaxShadowCastDistance);
		SetConsoleInt(TEXT("osm.Render.LocalLightMaxDynamicShadowCasters"), InSettings.LocalLightMaxDynamicShadowCasters);
		SetConsoleInt(TEXT("osm.Render.LocalLightShadowLodBias"), InSettings.LocalLightShadowLodBias);
		SetConsoleInt(TEXT("osm.Render.LocalLightMaxShadowSectionsPerLOD"), InSettings.LocalLightMaxShadowSectionsPerLOD);
	}
} // namespace OptimizedSkeletalMesh

void UOptimizedSkeletalMeshSettings::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject) || OptimizedSkeletalMesh::bRenderSettingsSyncInProgress)
	{
		return;
	}

	OptimizedSkeletalMesh::PushRenderSettingsToCVars(*this);
}

#if WITH_EDITOR
void UOptimizedSkeletalMeshSettings::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);
	if (OptimizedSkeletalMesh::bRenderSettingsSyncInProgress)
	{
		return;
	}

	OptimizedSkeletalMesh::PushRenderSettingsToCVars(*this);
	SaveConfig();
}
#endif
