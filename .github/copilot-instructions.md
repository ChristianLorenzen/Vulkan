# Project Guidelines

## Search First
- Search the codebase and existing docs before proposing a design or writing code.
- Treat the current branch's code as the source of truth. Planning docs can describe target architecture that is not fully implemented yet.
- Verify ownership and lifetime boundaries in the touched subsystem before changing code.

## Architecture
- Vulkan renderer code lives primarily in `src/Renderer/Vulkan`.
- Model import currently flows through `src/Renderer/Resources/Model.*`, with imported material data represented in `src/Renderer/Material/Material.hpp`.
- Engine entry points and sample scene wiring live in `src/Runtime/Engine.hpp`.
- Scene runtime code lives in `src/Scene`.
- Keep fixes aligned with the existing architecture on this branch. Do not invent or depend on asset, serialization, or runtime-service layers unless they exist in the workspace.

## Build And Validation
- Main application target: `faye_app`.
- After meaningful renderer, engine, or import changes, prefer validating with `cmake --build build --target faye_app`.
- If a full app build is not practical, run the broadest relevant targeted verification step and state what was not validated.

## Documentation
- After large renderer, Vulkan, or asset-import changes, update the most relevant existing doc in `docs/` or add a concise new status doc there.
- Record current behavior, important invariants, known limitations, and follow-up work.
- Keep documentation accurate to the current branch rather than the intended end state.

## Editing Conventions
- Keep changes minimal, focused, and consistent with the repository's C++20 and CMake style.
- Preserve existing scene and material behavior unless the task explicitly requires a behavioral change.
- Avoid editing vendored third-party code under `src/include` unless the task explicitly calls for it.