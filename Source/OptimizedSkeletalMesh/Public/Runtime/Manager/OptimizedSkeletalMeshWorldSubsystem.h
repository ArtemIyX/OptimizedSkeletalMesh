// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OptimizedSkeletalMeshTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "OptimizedSkeletalMeshWorldSubsystem.generated.h"

class USkeletalMesh;
class UAnimSequence;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class AActor;
class AOptimizedSkeletalMeshRenderBridgeActor;
class UOptimizedSkeletalMeshRenderComponent;

UCLASS()
class OPTIMIZEDSKELETALMESH_API UOptimizedSkeletalMeshWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	#pragma region Subsystem
	virtual void Initialize(FSubsystemCollectionBase& InCollection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float InDeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	#pragma endregion

	#pragma region InstanceLifecycle
	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Lifecycle", meta = (DisplayName = "Register Instance", Keywords = "instance register add remove update skeletal mesh"))
	FOptimizedSkeletalMeshInstanceHandle RegisterInstance(UPARAM(DisplayName = "Desc") const FOptimizedSkeletalMeshInstanceDesc& InDesc);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Lifecycle", meta = (DisplayName = "Unregister Instance", Keywords = "instance register add remove update skeletal mesh"))
	bool UnregisterInstance(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Lifecycle", meta = (DisplayName = "Add Instance", Keywords = "instance register add remove update skeletal mesh"))
	FOptimizedSkeletalMeshInstanceHandle AddInstance(UPARAM(DisplayName = "Desc") const FOptimizedSkeletalMeshInstanceDesc& InDesc);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Lifecycle", meta = (DisplayName = "Add Instances Batch", Keywords = "instance register add remove update skeletal mesh"))
	void AddInstancesBatch(UPARAM(DisplayName = "Base Desc") const FOptimizedSkeletalMeshInstanceDesc& InBaseDesc,UPARAM(DisplayName = "World Transforms") const TArray<FTransform>& InWorldTransforms,UPARAM(DisplayName = "Handles") TArray<FOptimizedSkeletalMeshInstanceHandle>& OutHandles);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Lifecycle", meta = (DisplayName = "Remove Instance", Keywords = "instance register add remove update skeletal mesh"))
	bool RemoveInstance(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Lifecycle", meta = (DisplayName = "Remove Instance By Id", Keywords = "instance register add remove update skeletal mesh"))
	bool RemoveInstanceById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Lifecycle", meta = (DisplayName = "Remove Instances", Keywords = "instance register add remove update skeletal mesh"))
	int32 RemoveInstances(UPARAM(DisplayName = "Handles") const TArray<FOptimizedSkeletalMeshInstanceHandle>& InHandles);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Lifecycle", meta = (DisplayName = "Remove Instances By Id", Keywords = "instance register add remove update skeletal mesh"))
	int32 RemoveInstancesById(UPARAM(DisplayName = "Instance Ids") const TArray<int32>& InInstanceIds);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Lifecycle", meta = (DisplayName = "Update Instance", Keywords = "instance register add remove update skeletal mesh"))
	bool UpdateInstance(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Desc") const FOptimizedSkeletalMeshInstanceDesc& InDesc);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Lifecycle", meta = (DisplayName = "Set Instance Skeletal Mesh", Keywords = "instance register add remove update skeletal mesh"))
	bool SetInstanceSkeletalMesh(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Skeletal Mesh") USkeletalMesh* InSkeletalMesh,UPARAM(DisplayName = "Maintain Anim") bool bInMaintainAnim = true,UPARAM(DisplayName = "Maintain Render Params") bool bInMaintainRenderParams = true);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Lifecycle", meta = (DisplayName = "Set Instance Skeletal Mesh By Id", Keywords = "instance register add remove update skeletal mesh"))
	bool SetInstanceSkeletalMeshById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "Skeletal Mesh") USkeletalMesh* InSkeletalMesh,UPARAM(DisplayName = "Maintain Anim") bool bInMaintainAnim = true,UPARAM(DisplayName = "Maintain Render Params") bool bInMaintainRenderParams = true);
	#pragma endregion

	#pragma region InstanceTransform
	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Transform", meta = (DisplayName = "Update Instance Transform", Keywords = "instance transform location rotation scale move"))
	bool UpdateInstanceTransform(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "World Transform") const FTransform& InWorldTransform);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Transform", meta = (DisplayName = "Update Instance Transform By Id", Keywords = "instance transform location rotation scale move"))
	bool UpdateInstanceTransformById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "World Transform") const FTransform& InWorldTransform);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Transform", meta = (DisplayName = "Update Instances Transform", Keywords = "instance transform location rotation scale move"))
	int32 UpdateInstancesTransform(UPARAM(DisplayName = "Handles") const TArray<FOptimizedSkeletalMeshInstanceHandle>& InHandles,UPARAM(DisplayName = "World Transforms") const TArray<FTransform>& InWorldTransforms);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Transform", meta = (DisplayName = "Update Instances Transform By Id", Keywords = "instance transform location rotation scale move"))
	int32 UpdateInstancesTransformById(UPARAM(DisplayName = "Instance Ids") const TArray<int32>& InInstanceIds,UPARAM(DisplayName = "World Transforms") const TArray<FTransform>& InWorldTransforms);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Transform", meta = (DisplayName = "Set Instance Location", Keywords = "instance transform location rotation scale move"))
	bool SetInstanceLocation(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "World Location") const FVector& InWorldLocation);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Transform", meta = (DisplayName = "Set Instance Location By Id", Keywords = "instance transform location rotation scale move"))
	bool SetInstanceLocationById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "World Location") const FVector& InWorldLocation);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Transform", meta = (DisplayName = "Set Instance Rotation", Keywords = "instance transform location rotation scale move"))
	bool SetInstanceRotation(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "World Rotation") const FRotator& InWorldRotation);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Transform", meta = (DisplayName = "Set Instance Rotation By Id", Keywords = "instance transform location rotation scale move"))
	bool SetInstanceRotationById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "World Rotation") const FRotator& InWorldRotation);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Transform", meta = (DisplayName = "Set Instance Scale", Keywords = "instance transform location rotation scale move"))
	bool SetInstanceScale(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "World Scale3 D") const FVector& InWorldScale3D);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Transform", meta = (DisplayName = "Set Instance Scale By Id", Keywords = "instance transform location rotation scale move"))
	bool SetInstanceScaleById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "World Scale3 D") const FVector& InWorldScale3D);

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Instance Transform", meta = (DisplayName = "Get Instance Transform", Keywords = "instance transform location rotation scale move"))
	bool GetInstanceTransform(FOptimizedSkeletalMeshInstanceHandle InHandle, FTransform& OutWorldTransform) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Instance Transform", meta = (DisplayName = "Get Instance Transform By Id", Keywords = "instance transform location rotation scale move"))
	bool GetInstanceTransformById(int32 InInstanceId, FTransform& OutWorldTransform) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Instance Transform", meta = (DisplayName = "Get Instance Location", Keywords = "instance transform location rotation scale move"))
	bool GetInstanceLocation(FOptimizedSkeletalMeshInstanceHandle InHandle, FVector& OutWorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Instance Transform", meta = (DisplayName = "Get Instance Location By Id", Keywords = "instance transform location rotation scale move"))
	bool GetInstanceLocationById(int32 InInstanceId, FVector& OutWorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Instance Transform", meta = (DisplayName = "Get Instance Rotation", Keywords = "instance transform location rotation scale move"))
	bool GetInstanceRotation(FOptimizedSkeletalMeshInstanceHandle InHandle, FRotator& OutWorldRotation) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Instance Transform", meta = (DisplayName = "Get Instance Rotation By Id", Keywords = "instance transform location rotation scale move"))
	bool GetInstanceRotationById(int32 InInstanceId, FRotator& OutWorldRotation) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Instance Transform", meta = (DisplayName = "Get Instance Scale", Keywords = "instance transform location rotation scale move"))
	bool GetInstanceScale(FOptimizedSkeletalMeshInstanceHandle InHandle, FVector& OutWorldScale3D) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Instance Transform", meta = (DisplayName = "Get Instance Scale By Id", Keywords = "instance transform location rotation scale move"))
	bool GetInstanceScaleById(int32 InInstanceId, FVector& OutWorldScale3D) const;
	#pragma endregion

	#pragma region InstanceVisibility
	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Visibility", meta = (DisplayName = "Update Instance Animation Time", Keywords = "instance visibility show hide animation playback"))
	bool UpdateInstanceAnimationTime(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Animation Time") float InAnimationTime);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Visibility", meta = (DisplayName = "Set Instance Animation Playing", Keywords = "instance visibility show hide animation playback"))
	bool SetInstanceAnimationPlaying(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Playing") bool bInPlaying);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Visibility", meta = (DisplayName = "Set Instance Animation Update Rate Hz", Keywords = "instance visibility show hide animation playback"))
	bool SetInstanceAnimationUpdateRateHz(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Animation Update Rate Hz") float InAnimationUpdateRateHz);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Visibility", meta = (DisplayName = "Set Instance Visible", Keywords = "instance visibility show hide animation playback"))
	bool SetInstanceVisible(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Visible") bool bInVisible);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Visibility", meta = (DisplayName = "Show Instance", Keywords = "instance visibility show hide animation playback"))
	bool ShowInstance(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Visibility", meta = (DisplayName = "Hide Instance", Keywords = "instance visibility show hide animation playback"))
	bool HideInstance(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Visibility", meta = (DisplayName = "Set Instances Visible", Keywords = "instance visibility show hide animation playback"))
	int32 SetInstancesVisible(UPARAM(DisplayName = "Handles") const TArray<FOptimizedSkeletalMeshInstanceHandle>& InHandles,UPARAM(DisplayName = "Visible") bool bInVisible);
	#pragma endregion

	#pragma region InstanceRendering
	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Custom Depth", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceCustomDepth(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Render Custom Depth") bool bInRenderCustomDepth,UPARAM(DisplayName = "Custom Depth Stencil Value") int32 InCustomDepthStencilValue = 0);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Custom Depth By Id", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceCustomDepthById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "Render Custom Depth") bool bInRenderCustomDepth,UPARAM(DisplayName = "Custom Depth Stencil Value") int32 InCustomDepthStencilValue = 0);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Render Custom Depth", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceRenderCustomDepth(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Render Custom Depth") bool bInRenderCustomDepth);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Render Custom Depth By Id", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceRenderCustomDepthById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "Render Custom Depth") bool bInRenderCustomDepth);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Custom Depth Stencil Value", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceCustomDepthStencilValue(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Custom Depth Stencil Value") int32 InCustomDepthStencilValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Custom Depth Stencil Value By Id", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceCustomDepthStencilValueById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "Custom Depth Stencil Value") int32 InCustomDepthStencilValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterial(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Material") UMaterialInterface* InMaterial);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material By Id", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterialById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "Material") UMaterialInterface* InMaterial);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material Scalar Param", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterialScalarParam(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Param Index") int32 InParamIndex,UPARAM(DisplayName = "Value") float InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material Scalar Param By Id", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterialScalarParamById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "Param Index") int32 InParamIndex,UPARAM(DisplayName = "Value") float InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material Vector Param", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterialVectorParam(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Start Param Index") int32 InStartParamIndex,UPARAM(DisplayName = "Value") const FVector& InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material Vector Param By Id", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterialVectorParamById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "Start Param Index") int32 InStartParamIndex,UPARAM(DisplayName = "Value") const FVector& InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material Bool Param", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterialBoolParam(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Param Index") int32 InParamIndex,UPARAM(DisplayName = "Value") bool bInValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material Bool Param By Id", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterialBoolParamById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "Param Index") int32 InParamIndex,UPARAM(DisplayName = "Value") bool bInValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material Texture Param", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterialTextureParam(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Parameter Name") FName InParameterName,UPARAM(DisplayName = "Texture") UTexture2D* InTexture);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material Texture Param By Id", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterialTextureParamById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "Parameter Name") FName InParameterName,UPARAM(DisplayName = "Texture") UTexture2D* InTexture);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material Scalar Param By Name", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterialScalarParamByName(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Parameter Name") FName InParameterName,UPARAM(DisplayName = "Value") float InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material Scalar Param By Name Id", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterialScalarParamByNameId(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "Parameter Name") FName InParameterName,UPARAM(DisplayName = "Value") float InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material Vector Param By Name", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterialVectorParamByName(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Parameter Name") FName InParameterName,UPARAM(DisplayName = "Value") const FVector& InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material Vector Param By Name Id", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterialVectorParamByNameId(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "Parameter Name") FName InParameterName,UPARAM(DisplayName = "Value") const FVector& InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material Bool Param By Name", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterialBoolParamByName(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Parameter Name") FName InParameterName,UPARAM(DisplayName = "Value") bool bInValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material Bool Param By Name Id", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterialBoolParamByNameId(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "Parameter Name") FName InParameterName,UPARAM(DisplayName = "Value") bool bInValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material Color Param", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterialColorParam(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Start Param Index") int32 InStartParamIndex,UPARAM(DisplayName = "Value") const FLinearColor& InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material Color Param By Id", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterialColorParamById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "Start Param Index") int32 InStartParamIndex,UPARAM(DisplayName = "Value") const FLinearColor& InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material Color Param By Name", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterialColorParamByName(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Parameter Name") FName InParameterName,UPARAM(DisplayName = "Value") const FLinearColor& InValue);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Rendering", meta = (DisplayName = "Set Instance Material Color Param By Name Id", Keywords = "instance rendering material custom depth stencil parameter color texture"))
	bool SetInstanceMaterialColorParamByNameId(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "Parameter Name") FName InParameterName,UPARAM(DisplayName = "Value") const FLinearColor& InValue);
	#pragma endregion

	#pragma region InstanceQuery
	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Instance Query", meta = (DisplayName = "Get Instance", Keywords = "instance query get state snapshot"))
	bool GetInstance(FOptimizedSkeletalMeshInstanceHandle InHandle, FOptimizedSkeletalMeshInstanceDesc& OutDesc) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Instance Query", meta = (DisplayName = "Get Instance By Id", Keywords = "instance query get state snapshot"))
	bool GetInstanceById(int32 InInstanceId, FOptimizedSkeletalMeshInstanceDesc& OutDesc) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Instance Query", meta = (DisplayName = "Get Instance Socket Transform", Keywords = "instance socket transform bone gameplay"))
	bool GetInstanceSocketTransform(
		UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,
		UPARAM(DisplayName = "Socket Name") FName InSocketName,
		FTransform& OutWorldTransform) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Instance Query", meta = (DisplayName = "Get Instance Socket Transform By Id", Keywords = "instance socket transform bone gameplay"))
	bool GetInstanceSocketTransformById(
		UPARAM(DisplayName = "Instance Id") int32 InInstanceId,
		UPARAM(DisplayName = "Socket Name") FName InSocketName,
		FTransform& OutWorldTransform) const;
	#pragma endregion

	#pragma region InstanceAttachment
	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Attachment", meta = (DisplayName = "Attach Instance To Instance", Keywords = "attach socket weapon child parent"))
	bool AttachInstanceToInstance(
		UPARAM(DisplayName = "Child Handle") FOptimizedSkeletalMeshInstanceHandle InChildHandle,
		UPARAM(DisplayName = "Parent Handle") FOptimizedSkeletalMeshInstanceHandle InParentHandle,
		UPARAM(DisplayName = "Socket Name") FName InSocketName,
		UPARAM(DisplayName = "Attachment") const FOptimizedSkeletalMeshInstanceAttachment& InAttachment,
		UPARAM(DisplayName = "Snap Now") bool bInSnapNow = true);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Attachment", meta = (DisplayName = "Attach Instance To Instance By Id", Keywords = "attach socket weapon child parent"))
	bool AttachInstanceToInstanceById(
		UPARAM(DisplayName = "Child Instance Id") int32 InChildInstanceId,
		UPARAM(DisplayName = "Parent Instance Id") int32 InParentInstanceId,
		UPARAM(DisplayName = "Socket Name") FName InSocketName,
		UPARAM(DisplayName = "Attachment") const FOptimizedSkeletalMeshInstanceAttachment& InAttachment,
		UPARAM(DisplayName = "Snap Now") bool bInSnapNow = true);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Attachment", meta = (DisplayName = "Detach Instance", Keywords = "detach unattach child"))
	bool DetachInstance(UPARAM(DisplayName = "Child Handle") FOptimizedSkeletalMeshInstanceHandle InChildHandle);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Attachment", meta = (DisplayName = "Detach Instance By Id", Keywords = "detach unattach child"))
	bool DetachInstanceById(UPARAM(DisplayName = "Child Instance Id") int32 InChildInstanceId);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Attachment", meta = (DisplayName = "Detach Children", Keywords = "detach children parent"))
	int32 DetachChildren(UPARAM(DisplayName = "Parent Handle") FOptimizedSkeletalMeshInstanceHandle InParentHandle);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Attachment", meta = (DisplayName = "Detach Children By Id", Keywords = "detach children parent"))
	int32 DetachChildrenById(UPARAM(DisplayName = "Parent Instance Id") int32 InParentInstanceId);

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Instance Attachment", meta = (DisplayName = "Is Instance Attached", Keywords = "is attached"))
	bool IsInstanceAttached(UPARAM(DisplayName = "Child Handle") FOptimizedSkeletalMeshInstanceHandle InChildHandle) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Instance Attachment", meta = (DisplayName = "Is Instance Attached By Id", Keywords = "is attached"))
	bool IsInstanceAttachedById(UPARAM(DisplayName = "Child Instance Id") int32 InChildInstanceId) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Instance Attachment", meta = (DisplayName = "Get Instance Attachment", Keywords = "get attachment parent socket"))
	bool GetInstanceAttachment(
		UPARAM(DisplayName = "Child Handle") FOptimizedSkeletalMeshInstanceHandle InChildHandle,
		FOptimizedSkeletalMeshInstanceAttachment& OutAttachment) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Instance Attachment", meta = (DisplayName = "Get Instance Attachment By Id", Keywords = "get attachment parent socket"))
	bool GetInstanceAttachmentById(
		UPARAM(DisplayName = "Child Instance Id") int32 InChildInstanceId,
		FOptimizedSkeletalMeshInstanceAttachment& OutAttachment) const;

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Attachment", meta = (DisplayName = "Set Attachment Follow Rotation", Keywords = "attach follow rotation"))
	bool SetAttachmentFollowRotation(
		UPARAM(DisplayName = "Child Handle") FOptimizedSkeletalMeshInstanceHandle InChildHandle,
		UPARAM(DisplayName = "Follow Rotation") bool bInFollowRotation);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Attachment", meta = (DisplayName = "Set Attachment Follow Rotation By Id", Keywords = "attach follow rotation"))
	bool SetAttachmentFollowRotationById(
		UPARAM(DisplayName = "Child Instance Id") int32 InChildInstanceId,
		UPARAM(DisplayName = "Follow Rotation") bool bInFollowRotation);
	#pragma endregion

	#pragma region InstanceAnimation
	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Animation", meta = (DisplayName = "Set Instance Animation Asset", Keywords = "instance animation play pause stop loop rate time"))
	bool SetInstanceAnimationAsset(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Animation") UAnimSequence* InAnimation,UPARAM(DisplayName = "Reset Time") bool bInResetTime = true);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Animation", meta = (DisplayName = "Set Instance Animation Asset By Id", Keywords = "instance animation play pause stop loop rate time"))
	bool SetInstanceAnimationAssetById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "Animation") UAnimSequence* InAnimation,UPARAM(DisplayName = "Reset Time") bool bInResetTime = true);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Animation", meta = (DisplayName = "Play Instance Animation", Keywords = "instance animation play pause stop loop rate time"))
	bool PlayInstanceAnimation(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Animation", meta = (DisplayName = "Play Instance Animation By Id", Keywords = "instance animation play pause stop loop rate time"))
	bool PlayInstanceAnimationById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Animation", meta = (DisplayName = "Pause Instance Animation", Keywords = "instance animation play pause stop loop rate time"))
	bool PauseInstanceAnimation(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Animation", meta = (DisplayName = "Pause Instance Animation By Id", Keywords = "instance animation play pause stop loop rate time"))
	bool PauseInstanceAnimationById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Animation", meta = (DisplayName = "Stop Instance Animation", Keywords = "instance animation play pause stop loop rate time"))
	bool StopInstanceAnimation(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Reset Time") bool bInResetTime = true);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Animation", meta = (DisplayName = "Stop Instance Animation By Id", Keywords = "instance animation play pause stop loop rate time"))
	bool StopInstanceAnimationById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "Reset Time") bool bInResetTime = true);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Animation", meta = (DisplayName = "Set Instance Animation Looping", Keywords = "instance animation play pause stop loop rate time"))
	bool SetInstanceAnimationLooping(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Loop Animation") bool bInLoopAnimation);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Animation", meta = (DisplayName = "Set Instance Animation Looping By Id", Keywords = "instance animation play pause stop loop rate time"))
	bool SetInstanceAnimationLoopingById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "Loop Animation") bool bInLoopAnimation);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Animation", meta = (DisplayName = "Set Instance Animation Play Rate", Keywords = "instance animation play pause stop loop rate time"))
	bool SetInstanceAnimationPlayRate(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Animation Play Rate") float InAnimationPlayRate);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Animation", meta = (DisplayName = "Set Instance Animation Play Rate By Id", Keywords = "instance animation play pause stop loop rate time"))
	bool SetInstanceAnimationPlayRateById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "Animation Play Rate") float InAnimationPlayRate);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Animation", meta = (DisplayName = "Set Instance Animation Time", Keywords = "instance animation play pause stop loop rate time"))
	bool SetInstanceAnimationTime(UPARAM(DisplayName = "Handle") FOptimizedSkeletalMeshInstanceHandle InHandle,UPARAM(DisplayName = "Animation Time") float InAnimationTime);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Instance Animation", meta = (DisplayName = "Set Instance Animation Time By Id", Keywords = "instance animation play pause stop loop rate time"))
	bool SetInstanceAnimationTimeById(UPARAM(DisplayName = "Instance Id") int32 InInstanceId,UPARAM(DisplayName = "Animation Time") float InAnimationTime);
	#pragma endregion

	#pragma region StatsAndSettings
	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Stats and Settings", meta = (DisplayName = "Get Instances Snapshot", Keywords = "stats settings render debug"))
	void GetInstancesSnapshot(TArray<FOptimizedSkeletalMeshInstanceSnapshot>& OutInstances) const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Stats and Settings", meta = (DisplayName = "Get Instance Count", Keywords = "stats settings render debug"))
	int32 GetInstanceCount() const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Stats and Settings", meta = (DisplayName = "Get Visible Render Batch Count", Keywords = "stats settings render debug"))
	int32 GetVisibleRenderBatchCount() const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Stats and Settings", meta = (DisplayName = "Get Last Animation Stats", Keywords = "stats settings render debug"))
	FOptimizedSkeletalMeshAnimationStats GetLastAnimationStats() const;

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Stats and Settings", meta = (DisplayName = "Get Last Render Stats", Keywords = "stats settings render debug"))
	FOptimizedSkeletalMeshRenderStats GetLastRenderStats() const;

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Stats and Settings", meta = (DisplayName = "Apply Render Settings", Keywords = "stats settings render debug"))
	void ApplyRenderSettings(UPARAM(DisplayName = "Settings") const FOptimizedSkeletalMeshRenderSettings& InSettings);

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Stats and Settings", meta = (DisplayName = "Get Render Settings", Keywords = "stats settings render debug"))
	FOptimizedSkeletalMeshRenderSettings GetRenderSettings() const;

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Stats and Settings", meta = (DisplayName = "Reload Render Settings From CVars", Keywords = "stats settings render debug"))
	void ReloadRenderSettingsFromCVars();

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Stats and Settings", meta = (DisplayName = "Register External Render Component", Keywords = "stats settings render debug"))
	void RegisterExternalRenderComponent(UPARAM(DisplayName = "Component") UOptimizedSkeletalMeshRenderComponent* InComponent);

	UFUNCTION(BlueprintCallable, Category = "Optimized Skeletal Mesh|Stats and Settings", meta = (DisplayName = "Unregister External Render Component", Keywords = "stats settings render debug"))
	void UnregisterExternalRenderComponent(UPARAM(DisplayName = "Component") UOptimizedSkeletalMeshRenderComponent* InComponent);
	#pragma endregion

	#pragma region RenderBridge
	void ApplyRenderSettingsToComponent(UOptimizedSkeletalMeshRenderComponent* InComponent) const;

	const TArray<FMatrix44f>* GetInstanceBonePalette(FOptimizedSkeletalMeshInstanceHandle InHandle) const;
	float GetInstanceAnimationBlendAlpha(FOptimizedSkeletalMeshInstanceHandle InHandle) const;
	void GetBonePaletteSnapshots(TArray<FOptimizedSkeletalMeshBonePaletteSnapshot>& OutSnapshots) const;
	void UpdateRenderVisibleInstanceIds(TConstArrayView<int32> InVisibleInstanceIds);
	void UpdateLastRenderStats(const FOptimizedSkeletalMeshRenderStats& InStats);

	UFUNCTION(BlueprintPure, Category = "Optimized Skeletal Mesh|Render Bridge", meta = (DisplayName = "Get Cached Bone Palette Count", Keywords = "render bridge bone palette cache"))
	int32 GetCachedBonePaletteCount() const;

	bool HasDirtyBonePalettes() const
	{
		return HasDirtyRenderVisibleBonePalettes();
	}

	void ClearDirtyBonePalettes()
	{
		ClearDirtyRenderVisibleBonePalettes();
	}

	bool IsRenderDataDirty() const
	{
		return bRenderDataDirty || bCustomDepthRenderDataDirty;
	}

	void SetExternalRenderBridgeActive(bool bInActive);

	void ClearRenderDataDirty()
	{
		bRenderDataDirty = false;
		bCustomDepthRenderDataDirty = false;
	}
	#pragma endregion

private:
	#pragma region Rendering
	void EnsureRenderBridge();
	void DestroyRenderBridge();
	void ApplyRenderSettingsToComponent();
	void RefreshActiveRenderSettings(bool bInForce);
	void RefreshCustomDepthRenderComponents();
	void RequestRenderRefreshForAllComponents();
	void RequestCustomDepthRenderRefresh();
	bool PushBonePalettesToRenderComponents();
	bool PushInstanceTransformsToRenderComponents();
	#pragma endregion

	#pragma region Instances
	int32 AllocateInstanceId();
	bool IsValidInstanceId(int32 InInstanceId) const;
	void MarkRenderDataDirty();
	void MarkCustomDepthRenderDataDirty();
	void MarkTransformDirty(int32 InInstanceId);
	bool HasDirtyTransforms() const;
	void ClearDirtyTransforms();
	void RefreshAnimationTracking(int32 InInstanceId, const FOptimizedSkeletalMeshInstanceDesc& InDesc, bool bInForceDirty);
	void RemoveAnimationTracking(int32 InInstanceId);
	void MarkBonePaletteDirty(int32 InInstanceId);
	bool HasDirtyRenderVisibleBonePalettes() const;
	void ClearDirtyRenderVisibleBonePalettes();
	bool AttachInstanceInternal(
		int32 InChildInstanceId,
		int32 InParentInstanceId,
		FName InSocketName,
		const FOptimizedSkeletalMeshInstanceAttachment& InAttachment,
		bool bInSnapNow);
	bool DetachInstanceInternal(int32 InChildInstanceId);
	bool WouldCreateAttachmentCycle(int32 InChildInstanceId, int32 InParentInstanceId) const;
	void RemoveAttachmentMapsForChild(int32 InChildInstanceId);
	void RemoveAttachmentMapsForParent(int32 InParentInstanceId);
	bool ResolveNamedMaterialParamSlot(FName InParameterName, int32 InWidth, int32& OutStartIndex);
	float GetEffectiveAnimationUpdateRateHz(const FOptimizedSkeletalMeshInstanceDesc& InDesc, float InNearestCameraDistance) const;
	static float GetUpdateRateScaleForDistance(float InDistance);
	bool GetNearestCameraDistance(const FVector& InWorldLocation, float& OutDistance) const;
	static bool ShouldTickAnimation(const FOptimizedSkeletalMeshInstanceDesc& InDesc);
	#pragma endregion

	#pragma region Animation
	void TickAnimation(float InDeltaTime);
	void InitializeAnimationStats(FOptimizedSkeletalMeshAnimationStats& OutStats, float InDeltaTime) const;
	void FinalizeAnimationStats(FOptimizedSkeletalMeshAnimationStats& OutStats) const;
	void BuildAnimationInstanceIdsToProcess(TArray<int32>& OutInstanceIdsToProcess, TArray<int32>& OutDirtyInstanceIdsToProcess) const;
	void BuildAnimationEvaluationWork(
		float InDeltaTime,
		TConstArrayView<int32> InInstanceIdsToProcess,
		FOptimizedSkeletalMeshAnimationStats& OutStats,
		TArray<FOptimizedSkeletalMeshAnimationEvaluationWork>& OutEvaluationWork);
	bool AdvanceAnimationTime(
		int32 InInstanceId,
		FOptimizedSkeletalMeshInstanceDesc& InOutDesc,
		float InAnimationDeltaTime,
		FOptimizedSkeletalMeshAnimationStats& OutStats);
	void RemoveInstanceAnimationData(int32 InInstanceId);
	void RunAnimationEvaluationWork(
		const TArray<FOptimizedSkeletalMeshAnimationEvaluationWork>& InEvaluationWork,
		FOptimizedSkeletalMeshAnimationStats& OutStats,
		TArray<FOptimizedSkeletalMeshAnimationEvaluationResult>& OutEvaluationResults) const;
	void ApplyAnimationEvaluationResults(
		TArray<FOptimizedSkeletalMeshAnimationEvaluationResult>& InOutEvaluationResults,
		FOptimizedSkeletalMeshAnimationStats& OutStats);
	void ClearProcessedDirtyAnimationIds(TConstArrayView<int32> InProcessedDirtyInstanceIds);
	FOptimizedSkeletalMeshAnimationMeshCache* FindOrBuildAnimationMeshCache(USkeletalMesh* InSkeletalMesh);
	bool EvaluateInstanceBonePalette(const FOptimizedSkeletalMeshInstanceDesc& InDesc, TArray<FMatrix44f>& OutBonePalette);
	static bool EvaluateInstanceBonePaletteWithCache(
		const FOptimizedSkeletalMeshInstanceDesc& InDesc,
		const FOptimizedSkeletalMeshAnimationMeshCache& InMeshCache,
		TArray<FMatrix44f>& OutBonePalette);
	static float WrapAnimationTime(float InAnimationTime, float InSequenceLength);
	#pragma endregion

	#pragma region Debug
	void DrawInstanceDebugOverlay() const;
	#pragma endregion

	#pragma region State
	UPROPERTY(Transient)
	TMap<int32, FOptimizedSkeletalMeshInstanceDesc> Instances;

	UPROPERTY(Transient)
	TArray<int32> FreeInstanceIds;

	UPROPERTY(Transient)
	TObjectPtr<AActor> RenderBridgeActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UOptimizedSkeletalMeshRenderComponent> RenderComponent = nullptr;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UOptimizedSkeletalMeshRenderComponent>> CustomDepthRenderComponents;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> MaterialTextureOverrideCache;

	UPROPERTY(Transient)
	TMap<FName, FIntPoint> NamedMaterialParamSlots;

	int32 NextNamedMaterialParamSlot = 0;

	TMap<TObjectKey<USkeletalMesh>, FOptimizedSkeletalMeshAnimationMeshCache> AnimationMeshCaches;
	TMap<int32, TArray<FMatrix44f>> PreviousInstanceBonePalettes;
	TMap<int32, TArray<FMatrix44f>> InstanceBonePalettes;
	TMap<int32, FOptimizedSkeletalMeshInstanceAttachment> ChildAttachments;
	TMultiMap<int32, int32> ParentToChildren;
	TMap<int32, float> InstanceAnimationBlendAlphas;
	TSet<int32> ActiveAnimationInstanceIds;
	TSet<int32> DirtyAnimationInstanceIds;
	TSet<int32> DirtyBonePaletteInstanceIds;
	TSet<int32> DirtyTransformInstanceIds;
	TSet<int32> RenderVisibleInstanceIds;
	TMap<int32, float> AnimationUpdateAccumulators;

	int32 NextInstanceId = 1;
	int32 BulkUpdateDepth = 0;
	int32 LastSeenRenderCVarVersion = 0;
	int32 RenderStateRecoveryAttempts = 0;
	mutable int32 CachedVisibleRenderBatchCount = 0;
	bool bRenderDataDirty = false;
	bool bCustomDepthRenderDataDirty = false;
	mutable bool bVisibleRenderBatchCountDirty = true;
	bool bExternalRenderBridgeActive = false;
	FOptimizedSkeletalMeshAnimationStats LastAnimationStats;
	FOptimizedSkeletalMeshRenderStats LastRenderStats;
	FOptimizedSkeletalMeshRenderSettings CurrentRenderSettings;
	FOptimizedSkeletalMeshRenderSettings ActiveRenderSettings;
	TArray<TWeakObjectPtr<UOptimizedSkeletalMeshRenderComponent>> ExternalRenderComponents;
	#pragma endregion
};
