// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FOptimizedSkeletalMeshModule : public IModuleInterface
{
public:
#pragma region IModuleInterface
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
#pragma endregion
};
