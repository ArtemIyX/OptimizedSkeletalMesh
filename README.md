# Optimized Skeletal Mesh Render - Plugin for Unreal Engine 5 

UE 5.7 plugin tested for high-count skeletal mesh rendering.

## What it does

Renders many animated skeletal meshes without one `USkeletalMeshComponent` and one AnimBP per entity.

The system uses:
- `UOptimizedSkeletalMeshWorldSubsystem` for instance state and gameplay API
- `UOptimizedSkeletalMeshRenderComponent` as a single render bridge
- custom GPU skinning path for batched render submission

## Features

- Add/remove/update instances by handle or id
- Per-instance animation control
- Per-instance transform control
- Per-instance material override
- Per-instance material parameter updates
- Custom depth and stencil per instance
- Attach one OSM instance to another by socket or bone
- Socket or bone transform query for gameplay
- Batch spawn support
- Debug overlay and stats


## Core API

Instance lifecycle:
- `RegisterInstance`
- `UnregisterInstance`
- `AddInstance`
- `AddInstancesBatch`

Transform:
- `UpdateInstanceTransform`
- `SetInstanceLocation`
- `SetInstanceRotation`
- `SetInstanceScale`
- `GetInstanceTransform`
- `GetInstanceLocation`
- `GetInstanceRotation`
- `GetInstanceScale`

Animation:
- `SetInstanceAnimationAsset`
- `PlayInstanceAnimation`
- `PauseInstanceAnimation`
- `StopInstanceAnimation`
- `SetInstanceAnimationLooping`
- `SetInstanceAnimationPlayRate`
- `SetInstanceAnimationTime`

Rendering:
- `SetInstanceMaterial`
- `SetInstanceCustomDepth`
- `SetInstanceCustomDepthStencilValue`
- material param setters

Attachment:
- `AttachInstanceToInstance`
- `DetachInstance`
- `DetachChildren`
- `IsInstanceAttached`
- `GetInstanceAttachment`

Query:
- `GetInstanceSocketTransform`

## Debug

Console commands:
- `osm.DebugEntities.Show`
- `osm.DebugEntities.Hide`
- `osm.DebugEntities.Toggle`

Stats:
- `stat OSMMeshes`
- `stat OSMVisibleLOD`
- `stat OSMAnimation`
- `stat OSMSkinning`
- `stat OSMShadows`

## Limitations

- No morph targets in the custom GPU skinning path
- No cloth support in this path
- Low update-rate animation can look stepped

## License

[MIT](LICENSE)
