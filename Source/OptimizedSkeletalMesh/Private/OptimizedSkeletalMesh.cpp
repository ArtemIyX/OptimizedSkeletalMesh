// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimizedSkeletalMesh.h"

#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FOptimizedSkeletalMeshModule"

void FOptimizedSkeletalMeshModule::StartupModule()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("OptimizedSkeletalMesh"));
	if (Plugin.IsValid())
	{
		AddShaderSourceDirectoryMapping(
			TEXT("/Plugin/OptimizedSkeletalMesh"),
			FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders")));
	}
}

void FOptimizedSkeletalMeshModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FOptimizedSkeletalMeshModule, OptimizedSkeletalMesh)
