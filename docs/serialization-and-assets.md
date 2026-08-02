# Scene Serialization & Asset Database — Status

Branch: `feature/scene-serialization` (2026-08-02). This doc records the
**implemented** state of scene save/load and the GUID asset database. The
normative file formats live in `docs/serialization/schemas.md`; the conformance
fixture is `assets/scenes/schema_example.faye`.

## What works

- **Stable entity GUIDs** — every `Ecs::Entity` gets a random v4 `Faye::Uuid` at
  creation (`EntityRegistry` keeps a parallel index-keyed `guids` vector plus a
  `Uuid → Entity` reverse map; the dense `{index, generation}` storage key is
  untouched). `World::createWithGuid` / `findByGuid` / `guidOf` support scene
  load. Duplicate entities and destroy/recreate mint fresh GUIDs.
- **GUID asset database** — `AssetDatabase` (`src/Assets/AssetDatabase.*`)
  stores `AssetRecord {id, type, name, sourceUri, primitiveName, persistence,
  dirty}`. Deterministic ids: file assets = `Uuid::nameBased(uri)`, primitives =
  `nameBased("primitive:<Name>")`, built-ins = `nameBased("builtin:<Name>")`.
  `ModelRegistry`/`MaterialRegistry` hold `AssetId ↔ handle` maps; primitives
  and file imports **dedupe** by id (`getOrImportByUri`).
- **YAML scene files** (`.faye`, schemaVersion 1) — `SceneFileWriter`/`Reader`
  serialize/deserialize entities (persisted GUIDs), components through the
  reserved `ComponentTypeInfo::serialize/deserialize` slots (hand-written
  thunks; field names are the C++ member names), and inline asset records
  (material data + pipeline inlined from the registries).
- **Save / Load / New** — `SceneManager::saveSceneToFile`/`loadSceneFromFile`
  (fill-in-place: the `Scene` object survives, so panels/script engines/extraction
  stay bound), Editor File menu (New/Save/Save As…/Open… with a path modal),
  deferred to end-of-frame. Startup: `--scene <path>` arg or `FAYE_SCENE` env
  loads a scene instead of the default; on failure it falls back to the
  `SceneBuilder::populate` default.

## Invariants

- Serialization keys on **registered component names** (`"Transform"`,
  `"Point Light"`, …), never `ComponentId` (first-use-ordered, unstable).
- Entity GUIDs and asset ids are stable across runs/machines; **do not hand-edit
  ids** in scene files (asset ids are derived from the uri, so a rewritten id
  breaks references).
- A loaded scene must contain exactly one primary camera entity
  (`Transform` + `Camera`, `primary: true`) and a post-process entity; the
  loader validates and reports errors otherwise. `SceneSetup{activeCamera,
  postProcessSettings}` is preserved for engine/editor consumption.
- Scene mutation (New/Open) runs only at **end of frame** (after `endFrame`) so
  no in-flight `RenderView`/snapshot dangles.
- `Core/ECS` stays headless: it only forward-declares `Serializer`/`Deserializer`
  (defined in `src/Scene/Serialization/`); no ImGui/Vulkan in core.

## Known limitations

- Loaded textures are **path-only references** (`Texture::create`); pixel data
  upload is deferred (water may not render correctly after reload until this is
  wired).
- Imported FBX materials are not persisted as records; they are reconstructed
  from the model import. Water's built-in material is re-registered on every
  `populate` (repeated `New` accumulates material handles — no removal API yet).
- The full scene round-trip is **not unit-tested** (registries need a live
  Vulkan device); component-level YAML round-trips are covered by
  `faye_tests`. Runtime validation: `bin/faye_app --scene <file.faye>`,
  File → Save / Open in the editor.
- `SceneBuilder::populate` runs on the main thread for New (heavy imports may
  stutter one frame).

## Follow-ups

- Field-description reflection layer (`REFLECT_FIELDS`-style) to replace the
  hand-written thunks (they fill the same reserved slots, so no restructuring).
- Runtime multi-scene switching (swap the `Scene` object + rebind panels, both
  script engines, engine-retained entities).
- Generic generational `Registry<T>`/`HandlePool<T>` for Model/Material/Template
  registries (`docs` forward-plan §5).
- Texture asset records in the AssetDatabase; `TextureRegistry` wiring.
- Persisted script component re-attachment edge cases (built-in scripts,
  hot-reload rebinding after load).
