// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimizedSkeletalMesh.h"
#include "OptimizedSkeletalMeshLog.h"

#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FOptimizedSkeletalMeshModule"

void FOptimizedSkeletalMeshModule::StartupModule()
{
	UE_LOG(LogOptimizedSkeletalMesh, Log, TEXT("OptimizedSkeletalMesh module startup"));
	const TSharedPtr<IPlugin> plugin = IPluginManager::Get().FindPlugin(TEXT("OptimizedSkeletalMesh"));
	if (plugin.IsValid())
	{
		AddShaderSourceDirectoryMapping(
			TEXT("/Plugin/OptimizedSkeletalMesh"),
			FPaths::Combine(plugin->GetBaseDir(), TEXT("Shaders")));
	}
}

void FOptimizedSkeletalMeshModule::ShutdownModule()
{
	UE_LOG(LogOptimizedSkeletalMesh, Log, TEXT("OptimizedSkeletalMesh module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FOptimizedSkeletalMeshModule, OptimizedSkeletalMesh)
