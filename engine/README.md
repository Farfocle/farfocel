# Engine

The core of Farfocel. Headers live in `include/fr/<module>/`, implementation in `src/<module>/`.

## Submodules

- **core** - foundational utilities: allocators, data structures (Array, HashMap, HashSet, String, ...), serialization, and misc helpers
- **data** - ECS: Things (entities), Parts (components), Systems, Scripts, World, queries, relations, and deferred commands
- **asset** - asset catalog and runtime asset loading
- **renderer** - rendering pipeline: RenderDevice (OpenGL), CommandBuffer, RenderQueue, and high-level Renderer
- **scene** - scene helpers: spawn actions, camera, picking, ImGui setup/teardown
- **devtools** - in-engine developer tools: inspector, panels, transform gizmo, ImGui serialization archive
- **platform** - window and input abstraction (SDL3)
- **physics** - physics (work in progress)
- **logger** - logging
