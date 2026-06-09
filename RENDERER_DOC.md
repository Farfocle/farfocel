# Farfocel Renderer Documentation

#### AI Warning
The initial draft of this document that I had written has been reviewed and altered by the AI to improve its layout and correctness.

## Overview

Farfocel's Renderer is built on top of the RHI 'RenderDevice'.
The frame is rendered in a fixed order:

```text
1. Shadow Pass
2. G-Buffer Geometry Pass
3. Lighting / Composition Pass
```

The current renderer supports:

- cooked mesh loading,
- cooked texture loading,
- deferred G-Buffer rendering,
- opaque and masked geometry,
- PBR and classic shading methods (currently, PBR is not fully finished),
- point lights,
- directional light,
- cascaded shadow maps for the first main directional light,
- HDR skybox display (no IBL lighting yet).

---

## Renderer Architecture

```text
Source Assets
    ↓
Asscooker
    ↓
Cooked Assets: .fmesh / .ftex
    ↓
AssetManager
    ↓
MeshData / TextureHandle
    ↓
ECS (World)
    ↓
RenderSystem
    ↓
RenderQueue
    ↓
Renderer
    ↓
RenderDevice
    ↓
OpenGL Backend
```

---

## Assets

Examples:

```text
model.gltf (the only way of loading models into the engine as of right now)
- albedo.png
- normal.png
- metallic_roughness.png

sky.hdr
```

The renderer does not use these files directly. They are first processed by the asset cooker.

---

## Asscooker

The asset cooker converts source asset files into Farfocel's binary formats.

Main functions:
```cpp
fr::asscooker::cook_mesh(input_path, output_path);
fr::asscooker::cook_texture(input_path, output_path, is_srgb);
```

The cooker produces:

```text
.fmesh
.ftex
```

The current cooked formats are uncompressed. This means cooked files are larger than the original `.png`, `.jpg`, `.hdr`, or `.gltf` files. This is expected as of right now, and will likely not change soon, as compressing assets makes the initial process much longer.

---

## Cooked Asset Formats

Cooked asset disk layouts are defined in:

```text
engine/include/fr/data/asset_format.hpp
```

This file contains disk format structures:

```cpp
AssetBaseHeader
TextureHeader
MeshHeader
CookedSubMesh
CookedVertex
```

These structures are not runtime assets and are not GPU resources. They are the binary contract between:

```text
asscooker writes them
AssetManager reads them
```

---

## Runtime Assets

Runtime assets records live in:

```text
engine/include/fr/data/asset_manager.hpp
```

Current Types:

```cpp
MeshAsset
TextureAsset
AssetManager
MeshAssetHandle
TextureAssetHandle
```

Runtime assets are records stored inside the `AssetManager`. They track:

- GPU resources,
- reference counts,
- path hashes,
- texture dependencies.

---

## Renderer GPU Data

Renderer mesh data is defined in:

```text
engine/include/fr/renderer/mesh.hpp
```

Current types:

```cpp
SubMesh
MeshData
```

`MeshData` contains GPU handles:

```cpp
BufferHandle vbo;
BufferHandle ibo;
DynamicArray<SubMesh> submeshes;
```

It does not own CPU mesh data. The actual GPU buffers are owned by the `RenderDevice`.

---

## RenderDevice

`RenderDevice` is the low level rendering backend interface (RHI).

File:

```text
engine/include/fr/renderer/render_device.hpp
```

The current implementation is OpenGL:

```text
engine/src/renderer/opengl/opengl_render_device.cpp
```

The plan is to in the future add a Vulkan implementation.

---

### Handles

GPU resources are referenced through something called strong handles:

```cpp
BufferHandle
TextureHandle
ShaderHandle
RenderPipelineHandle
```

These are type safe wrappers over slot-map keys.

Important note:

```cpp
handle.is_valid()
```

only checks whether the handle looks locally valid. It does not guarantee that the backend resource still exists. The backend validates handles against its internal resource storage.

---

### Buffer Creation

Buffers can be created through the descriptor based way:

```cpp
BufferDesc desc{};
desc.size = size_in_bytes;
desc.usage = BufferUsage::Dynamic;
desc.initial_data = {};

BufferHandle buffer = device->create_buffer(desc);
```

Old legacy functions still exist:

```cpp
device->create_buffer(data, false);
device->create_empty_buffer(size, true);
```

Typical usage:

```text
Mesh VBO/IBO:
    create_buffer(data, false)

Per-frame SSBO:
    create_empty_buffer(size, true)
```

---

### Texture Creation

Textures are created through `TextureDesc`:

```cpp
TextureDesc desc{};
desc.width = width;
desc.height = height;
desc.mip_levels = mip_count;
desc.format = TextureFormat::R8G8B8A8_SRGB;
desc.initial_data = pixel_data;

TextureHandle texture = device->create_texture_2d(desc);
```

Compatibility helper:

```cpp
device->create_texture_2d(width, height, mip_levels, format, data);
```

Common texture formats:

```cpp
TextureFormat::R8G8B8A8_UNorm
TextureFormat::R8G8B8A8_SRGB
TextureFormat::R16G16_Float
TextureFormat::R32G32B32A32_Float
TextureFormat::Depth32_Float
TextureFormat::Depth32_Float_Shadow
```

---

### Pipelines

A render pipeline combines:

- shader,
- culling mode,
- depth test state,
- depth write state,
- wireframe state.

Example:

```cpp
RenderPipelineProperties props{};
props.shader = shader;
props.cull_mode = CullMode::Back;
props.depth_test = true;
props.depth_write = true;
props.wireframe = false;

RenderPipelineHandle pipe = device->create_render_pipeline(props);
```

---

### CommandBuffer

Rendering commands are recorded into a `CommandBuffer`.

Important commands:

```cpp
begin_render_pass(...)
end_render_pass()
set_viewport(...)
set_pipeline(...)
bind_vertex_buffer(...)
bind_index_buffer(...)
bind_texture(...)
bind_storage_buffer(...)
set_push_constants(...)
draw_indexed(...)
draw_arrays(...)
```

The higher level `Renderer` records commands and submits them through:

```cpp
device->submit_command_buffer(cmd);
```

---

## Render Bindings

Shader binding constants are defined in:

```text
engine/include/fr/renderer/render_bindings.hpp
```

These constants must match GLSL `layout(binding = X)` declarations.

---

### SSBO Bindings

```cpp
SSBO_TRANSFORMS    = 0
SSBO_CAMERA        = 1
SSBO_POINT_LIGHTS  = 2
SSBO_DIR_LIGHTS    = 3
```

---

### Geometry Pass Texture Units

```cpp
TEX_ALBEDO = 0
TEX_NORMAL = 1
TEX_EXTRA  = 2
```

---

### Lighting Pass Texture Units

```cpp
GBUFFER_ALBEDO = 0
GBUFFER_NORMAL = 1
GBUFFER_EXTRA  = 2
GBUFFER_DEPTH  = 3
SHADOW_MAP     = 4
IBL_SKYBOX     = 5
```

---

## RenderQueue

File:

```text
engine/include/fr/renderer/render_queue.hpp
```

`RenderQueue` collects renderable data for one frame.

It stores:

```cpp
DynamicArray<DrawCall> m_packets;
DynamicArray<glm::mat4> m_transforms;
DynamicArray<PointLightData> m_point_lights;
DynamicArray<DirectionalLightData> m_directional_lights;
```

---

### DrawCall

A `DrawCall` contains everything required to draw one submesh:

```cpp
RenderPipelineHandle pipe;
BufferHandle vbo;
BufferHandle ibo;
TextureHandle albedo_map;
TextureHandle normal_map;
TextureHandle extra_map;
U32 index_count;
U32 index_offset;
U32 vertex_offset;
U32 vbo_stride;
U32 transform_index;
U32 shading_model;
```

The transform matrix itself is stored separately in the queue transform array. `transform_index` points into that array.

---

### SortKey

Draw calls are sorted using a packed 64-bit key:

```text
[61..63] Pass type
[49..60] Pipeline
[33..48] Vertex buffer
[18..32] Texture
[0..17]  Depth
```

This is the main rendering optimization done in Farfocel. I am not crazy enough to do frame-graphs, but, who knows, maybe in the future...
This helps reduce GPU state changes.

Current renderer mostly uses this for grouping opaque and masked geometry.

Transparent sorting is not implemented yet but will definitely be.

---

### Sending Draw Calls

To add a draw call:

```cpp
queue.send_call(call, model_matrix);
```

This stores the transform matrix and assigns `call.transform_index`.

---

### Sending Lights

Point lights:

```cpp
queue.send_point_light(light_data);
```

Directional lights:

```cpp
queue.send_directional_light(light_data);
```

The renderer uploads light data from the geometry queue.

---

## RenderSystem

File:

```text
engine/include/fr/scene/render_system.hpp
```

`RenderSystem` converts ECS scene data into render queues.

It does not render itself, it's a bridge between ECS and renderer.

---

### Responsibilities

```text
World + AssetManager
    ↓
RenderSystem
    ↓
RenderQueue
```

It handles:

- camera extraction,
- mesh submission,
- frustum culling,
- shadow caster submission,
- point light submission,
- directional light and cascade data submission.

---

### Camera Extraction

```cpp
CamData cam = RenderSystem::extract_cam_data(world, aspect_ratio);
```

`CamData` contains:

```cpp
glm::mat4 view_proj;
glm::vec3 pos;
glm::vec3 dir;
```

---

### Geometry Submission

```cpp
RenderStats stats = RenderSystem::submit_meshes(
    world,
    geom_queue,
    assets,
    gbuffer_pipeline,
    cam.view_proj
);
```

This submits visible opaque and masked submeshes into the geometry queue.

It performs CPU frustum culling using transformed submesh AABBs, which is also a standard optimization.

Transparent and UI geometry are currently skipped, but a forward renderer will be added.

---

### Shadow Submission

```cpp
RenderStats shadow_stats = RenderSystem::submit_shadow_casters(
    world,
    shadow_queue,
    assets,
    shadow_pipeline
);
```

This submits shadow casting geometry.

Currently:

```text
Opaque      → casts shadows
Masked      → casts shadows
Transparent → skipped
UI          → skipped
```

The shadow path does not use camera frustum culling because objects outside the camera view may still cast visible shadows.

---

### Point Light Submission

```cpp
RenderSystem::submit_lights(world, geom_queue);
```

This collects ECS things (aka entities) with:

```cpp
PointLightPart
TransformPart
```

and writes them into the queue.

---

### Directional Light Submission

```cpp
RenderSystem::submit_directional_lights(
    world,
    geom_queue,
    cam.pos,
    cam.dir,
    shadow_settings
);
```

This collects directional lights and computes cascaded shadow matrices.

Only the first directional light is currently used for shadow lookup in the lighting shader, so the first directional light added to the scene acts as the sun.

---

## Directional Shadows

Directional shadow settings are represented by:

```cpp
DirectionalShadowSettings
```

Important fields:

```cpp
cascade_splits
cascade_half_extents
cascade_depth_ranges
min_bias
slope_bias
cascade_bias_scale
shadow_strength
```

---

### Cascades

The renderer currently uses 3 cascades stored in a 2x2 atlas.

I know the current implementation of cascaded shadows is laking, but it will definitely be improved once I learn more about the technique. Important to note that I am implementing a lot of things in the renderer for the first time in my life;

Atlas layout:

```text
Cascade 0: bottom-left quadrant
Cascade 1: bottom-right quadrant
Cascade 2: top-left quadrant
Fourth quadrant unused
```

The shadow atlas size is currently:

```cpp
4096 x 4096
```

Each cascade receives:

```cpp
2048 x 2048
```

---

### Bias Settings

Shadow bias parameters are passed to the lighting shader through:

```cpp
DirectionalLightData::shadow_params
```

Layout:

```text
x = minimum bias
y = slope bias
z = cascade bias scale
w = shadow strength
```

These help tune:

```text
shadow acne
peter-panning
shadow intensity
cascade artifacts
```

---

## Renderer

File:

```text
engine/include/fr/renderer/renderer.hpp
```

The `Renderer` owns persistent rendering resources:

```cpp
RendererGlobalBuffers
RendererFallbackTextures
GBufferTargets
ShadowResources
```

It does not own:

- mesh assets,
- texture assets,
- shaders,
- pipelines,
- ECS data.

---

### RenderFrameDesc

A frame is rendered using:

```cpp
RenderFrameDesc frame{};
frame.geom_queue = &geom_queue;
frame.shadow_queue = &shadow_queue;
frame.width = width;
frame.height = height;
frame.view_proj = cam.view_proj;
frame.cam_pos = cam.pos;
frame.cam_forward = cam.dir;
frame.lighting_pipe = lighting_pipe;
frame.shadow_pipe = shadow_pipe;
frame.skybox_map = skybox_texture;

renderer.render(frame);
```

---

### Renderer-Owned Resources

#### Global Buffers

```cpp
RendererGlobalBuffers
```

Contains:

```cpp
transform_ssbo
shadow_transform_ssbo
camera_ssbo
point_lights_ssbo
dir_lights_ssbo
```

These are updated every frame.

---

#### Fallback Textures

```cpp
RendererFallbackTextures
```

Contains:

```cpp
white
black
normal
material
```

Fallback usage:

```text
missing albedo         → white
missing normal         → default normal
missing material/extra → default material
missing skybox         → black
```

Default material layout:

```text
R = metallic/specular parameter = 0
G = roughness                  = 1
B = ambient occlusion          = 1
A = unused                     = 1
```

---

#### GBufferTargets

```cpp
GBufferTargets
```

Contains:

```cpp
albedo
normal
extra
depth
width
height
```

G-Buffer layout:

```text
Albedo:
    RGBA8 UNorm
    linear RGB albedo
    alpha/mask

Normal:
    RG16F
    octahedral encoded world normal

Extra:
    RGBA8 UNorm
    R = metallic/specular parameter
    G = roughness
    B = ambient occlusion
    A = shading model / 255

Depth:
    Depth32 Float
```

---

#### ShadowResources

```cpp
ShadowResources
```

Contains:

```cpp
TextureHandle map;
U32 size;
```

Current size:

```cpp
4096
```

---

## Frame Pipeline

A frame is rendered in this order:

```text
1. prepare_render_targets()
2. update_global_buffers()
3. execute_shadow_pass()
4. execute_geometry_pass()
5. execute_lighting_pass()
6. submit command buffer
```

---

### 1. Render Target Preparation

```cpp
prepare_render_targets(width, height)
```

This function:

- recreates G-Buffer textures when viewport size changes,
- creates the shadow map atlas.

G-Buffer is resized when:

```cpp
old_width != width || old_height != height
```
so when the window is resized.

---

### 2. Global Buffer Updates

```cpp
update_global_buffers(...)
```

Uploads:

- geometry transforms,
- shadow transforms,
- point lights,
- directional lights,
- camera data.

Camera data contains:

```cpp
view_proj
inv_view_proj
cam_pos
cam_forward
num_point_lights
num_dir_lights
```

`cam_forward` is used in the lighting shader for cascade selection as of right now.

---

### 3. Shadow Pass

```cpp
execute_shadow_pass(...)
```

The shadow pass:

- renders into the shadow map atlas,
- uses the shadow transform SSBO,
- uses directional light cascade matrices,
- renders three cascades,
- uses albedo alpha for masked geometry.

Shader files:

```text
engine/shaders/core/shadow.vert
engine/shaders/core/shadow.frag
```

Masked objects use alpha discard:

```glsl
if (texture(u_albedo_map, v_uv).a < 0.1) discard;
```

---

### 4. G-Buffer Geometry Pass

```cpp
execute_geometry_pass(...)
```

The geometry pass fills:

```text
GBuffer albedo
GBuffer normal
GBuffer extra
GBuffer depth
```

Shader files:

```text
engine/shaders/core/gbuffer.vert
engine/shaders/core/gbuffer.frag
```

The pass uses:

```cpp
TransformSSBO
CameraSSBO
albedo texture
normal texture
extra/material texture
```

---

### 5. Lighting / Composition Pass

```cpp
execute_lighting_pass(...)
```

This pass renders a fullscreen triangle directly to the default framebuffer.

Shader files:

```text
engine/shaders/core/lighting.vert
engine/shaders/core/lighting.frag
```

Inputs:

```text
GBuffer albedo
GBuffer normal
GBuffer extra
GBuffer depth
Shadow map
Skybox texture
Camera SSBO
Point lights SSBO
Directional lights SSBO
```

Outputs:

```text
Final LDR color to backbuffer
```

The pass applies:

```text
direct point light contribution
direct directional light contribution
shadow factor
simple constant ambient fallback
ACES tonemapping
gamma correction
skybox background
```

---

## Material System

The renderer currently supports three shading models:

```cpp
enum class ShadingModel {
    Unlit = 0,
    Standard = 1,
    PBR = 2
};
```

---

### Unlit

Unlit materials output albedo with ambient occlusion:

```text
color = albedo * ao
```

They are not affected by dynamic lights.

---

### Standard

Standard materials use a simple Blinn-Phong style model:

```text
ambient + diffuse + specular
```

Current Standard path:

```text
constant ambient
directional light
point lights
shadowing from main directional light
fixed specular response
```

The material extra texture reserves channels for future Standard parameters, but they are not fully used yet.

---

### PBR

PBR materials use a metallic/roughness direct lighting model.

Current PBR path:

```text
direct directional light
direct point lights
shadowing from main directional light
small constant ambient fallback
```

Full IBL is not implemented yet, as I mentioned.

---

## HDR Skybox

The editor can cook and load `.hdr` files as `.ftex` textures.

Current behavior:

```text
HDR skybox is displayed as background.
HDR skybox does not provide correct scene lighting, or any lighting.
```


---

## Asset Loading Flow

### Texture Loading

Texture loading flow:

```text
.ftex
    ↓
LoadedTextureFile
    ↓
RenderDevice::create_texture_2d()
    ↓
TextureAsset
    ↓
TextureAssetHandle
```

`LoadedTextureFile` is CPU-side validated file data.

`TextureAsset` is runtime asset manager data.

`TextureHandle` is a GPU resource handle.

---

### Mesh Loading

Mesh loading flow:

```text
.fmesh
    ↓
LoadedMeshFile
    ↓
RenderDevice::create_buffer()
    ↓
MeshData
    ↓
MeshAsset
    ↓
MeshAssetHandle
```

`LoadedMeshFile` contains:

```cpp
submeshes
vertices
indices
string_block
aabb_min
aabb_max
```

`MeshData` contains GPU handles and runtime submesh records.

---

## Editor Usage
The editor is home to some of the worst code written in the project, beware.

The editor is a renderer testbed.

Main features:

```text
Cook & Load GLTF
Cook & Load HDR
Wireframe toggle
Shading model selection
Directional light controls
Point light controls
Cascaded shadow controls
Render statistics
```

---

### Loading a Model

1. Enter a `.gltf` path.
2. Click:

```text
Cook & Load GLTF
```

If the cooked `.fmesh` exists, it is loaded directly.

If not, the editor cooks the asset asynchronously.

---

### Loading a Skybox

1. Enter a `.hdr` path.
2. Click:

```text
Cook & Load HDR
```

If the cooked `.ftex` exists, it is loaded directly.

If not, the editor cooks it asynchronously.

---

### Camera Controls

```text
Hold RMB:
    mouse look

WASD:
    movement

Space:
    move up

Left Shift:
    move down
```

---

### Renderer Diagnostics

The editor displays:

```text
Geometry total submeshes
Geometry visible submeshes
Geometry culled submeshes

Shadow total casters
Shadow submitted casters
Shadow skipped casters
```

---

## Typical Frame Setup

A typical frame in the editor looks like this:

```cpp
geom_queue.clear_leftover();
shadow_queue.clear_leftover();

CamData cam = RenderSystem::extract_cam_data(world, aspect);

RenderStats geometry_stats = RenderSystem::submit_meshes(
    world,
    geom_queue,
    assets,
    gbuffer_pipe,
    cam.view_proj
);

RenderStats shadow_stats = RenderSystem::submit_shadow_casters(
    world,
    shadow_queue,
    assets,
    shadow_pipe
);

RenderSystem::submit_lights(world, geom_queue);

RenderSystem::submit_directional_lights(
    world,
    geom_queue,
    cam.pos,
    cam.dir,
    shadow_settings
);

geom_queue.sort();
shadow_queue.sort();

RenderFrameDesc frame{};
frame.geom_queue = &geom_queue;
frame.shadow_queue = &shadow_queue;
frame.width = width;
frame.height = height;
frame.view_proj = cam.view_proj;
frame.cam_pos = cam.pos;
frame.cam_forward = cam.dir;
frame.lighting_pipe = lighting_pipe;
frame.shadow_pipe = shadow_pipe;
frame.skybox_map = skybox_texture;

renderer.render(frame);
```

---

## Known Limitations

### No Full IBL

HDR skybox rendering works, but full image-based lighting is not implemented yet.

Missing:

```text
irradiance convolution
prefiltered environment map
BRDF LUT
```

---

### No Transparent Pass

Transparent geometry is currently skipped by deferred geometry submission.

Future work:

```text
forward transparent pass
transparent sorting
alpha blending
```

---


### One Shadow-Casting Directional Light

Only the first directional light is used for cascaded shadow lookup.

Additional directional lights may contribute lighting in the future, but currently the shader uses:

```glsl
u_dir_lights[0]
```

for shadowing.

---

### Uncompressed Cooked Assets

Cooked `.ftex` and `.fmesh` files currently store simple runtime data.

This can be larger than source assets.

Future work:

```text
BC texture compression
mesh optimization
index size reduction
vertex format packing
```

---

### No Hot Reload

Assets can be reloaded through the editor UI, but there is no automatic file watching or hot reload system.

---


