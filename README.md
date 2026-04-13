# Custom C++ Engine and Editor

A personal C++ engine and editor I built for gameplay prototyping. Written from scratch in C++ with DirectX 11.

## What it does

The editor lets you build scenes with a hierarchy of objects, move them around with gizmos, paint terrain, and undo/redo everything via a command stack. The same scene file loads in a separate runtime launcher without the editor overhead.

The terrain system is the part I'm most happy with - the heightmap drives both the rendered mesh and the collision, so they're never out of sync.

- Scene hierarchy with drag and drop parenting
- Translate, rotate and scale gizmos (ImGuizmo, local space)
- Terrain tool with heightmap-based rendering and collision
- Command pattern undo/redo (Ctrl+Z / Ctrl+Shift+Z)
- Component-based scene graph with JSON serialization
- Rigidbody physics and suspension constraints
- Debug overlays for collision shapes and physics state

## Building

Requires Visual Studio 2022 and Windows SDK.

```
cd tools
generate_projects.bat
```

This generates `Tools.sln` at the repo root and the project files under `local/`. Build targets: **Editor** (full editor), **GameLauncher** (runtime only).

The root `build.bat` script can also build the solution directly once the project files exist.

## Where to look first

- `tools/source/Editor/` for the editor UI, scene tools, and command-based undo/redo flow
- `tools/source/Engine/GraphicsEngine/Terrain/` for the terrain renderer and shared heightmap/collision pipeline
- `tools/source/Projects/ExampleGame/` for the sample gameplay code and vehicle/autotest work

## Structure

```
tools/source/
  Editor/       - editor UI, tools, commands
  Engine/       - renderer, physics, scene, math
  GameLauncher/ - runtime executable
  Projects/     - sample game code and autotests
```
