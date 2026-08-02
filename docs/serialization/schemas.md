# Scene Serialization Schemas (v1)

Status: **DRAFT — normative spec for `feature/scene-serialization`** (2026-08-02).
This document is the single source of truth for the `.faye` scene file format, the
asset record schema, UUID conventions, component field definitions, and the
loader/versioning rules. The conformance fixture `assets/scenes/schema_example.faye`
is the executable example of this spec (parsed by the Phase-3 conformance test).

Design goals:
- **Reflection-ready**: field names are the C++ data member names, so a future
  `REFLECT_FIELDS`-style field-description layer emits identical YAML with no
  format churn. Component `type:` keys stay the registered display names
  (`"Transform"`, `"Point Light"`) because the ECS registry mandates name-keyed
  persistence (ComponentId is first-use-ordered and unstable across runs).
- **Stable identity**: entities and assets are identified by UUIDs persisted in
  the file; runtime handles (`ModelHandle`, `MaterialHandle`, `Ecs::Entity`)
  are never written.
- **Author-friendly**: YAML, not a database. Diffable, hand-editable, and
  versioned with explicit forward/backward-compat rules.

---

## 1. UUID conventions

- 128-bit RFC 4122 UUID. File representation is the canonical lowercase
  `8-4-4-4-12` string, e.g. `f47ac10b-58cc-4372-a567-0e02b2c3d479`.
  Never raw binary in text files.
- `Faye::Uuid` wraps `boost::uuids::uuid` (see `src/Core/Serialization/Uuid.hpp`).
  - **v4 (random)**: entity GUIDs, scene UUIDs, runtime-created asset IDs —
    `boost::uuids::random_generator_mt19937`.
  - **v5 (SHA-1, deterministic)**: file-sourced asset IDs —
    `boost::uuids::name_generator` over `"faye:" + sourceUri`. Stable across
    runs and machines; no sidecar DB required.
- Null UUID `00000000-0000-0000-0000-000000000000` = none/invalid.
- Well-known built-in asset IDs (primitives, Default Material, water material)
  are constants registered by `Engine::initialize`.

---

## 2. Scene file (`.faye`) — schemaVersion 1

Top-level structure:

```yaml
schemaVersion: 1        # required, first key; loader migrates <1, refuses >1
scene:
  uuid: <uuid>          # random v4, minted at first save
  name: <string>
assets:                 # asset records referenced by this scene (may be empty)
  - <asset record>      # see §3
entities:
  - <entity>            # see §2.1
```

### 2.1 Entity

```yaml
id: <uuid>              # required, unique; entity GUID (v4, minted at create)
name: <string>          # optional; default "Entity <n>"
components:
  - type: <registered-name>   # "Transform", "Point Light", ... (never ComponentId)
    <field>: <value>          # C++ member names, see §4
```

### 2.2 Loader rules

- `schemaVersion` must be present and parse as an integer.
- Entity `id` is required and must be unique within the file; duplicates or a
  missing id are load errors.
- `name` is optional; a missing name gets `"Entity <n>"` (next ordinal).
- Component lookup is by **registered name** (`ComponentTypeRegistry`); an
  unknown component `type` → **warn + skip** (forward compatibility).
- Unknown fields inside a known component → **warn + skip**.
- Missing fields → component defaults (default-construct, then overlay the
  present fields).
- Entity order is preserved and drives `Scene::sceneEntities` / hierarchy order.

---

## 3. Asset records

Inline `assets:` section in v1; standalone `.fasset` files are a future split
(the record schema below is identical in both forms).

```yaml
id: <uuid>                    # required; v5 for uri-sourced, v4 for runtime-created
type: model | material        # texture deferred (stays inline in MaterialData)
name: <string>                # optional display name
uri: <string>                 # optional; file-sourced assets only
primitive: <string>           # models only: cube|sphere|plane|quad (mutually exclusive with uri)
flags: [builtin | imported | transient]   # optional
data: { ... }                 # type-specific property list (see §3.1 / §3.2)
pipeline: { ... }             # materials only: MaterialPipelineConfig (see §3.2)
```

C++ mapping: `AssetRecord { AssetId id; AssetType type; std::string sourceUri;
std::string name; AssetFlags flags; bool dirty; }` (`src/Assets/AssetDatabase.hpp`).

Notes:
- Imported FBX materials are **not** separate records — they are reconstructed
  deterministically from the model import. Only explicitly created materials
  (SceneBuilder named materials, editor-created ones) get records.
- `AssetId` for a file-sourced asset is `Uuid::nameBased("faye:" + uri)`; the
  loader can therefore re-derive the id from the uri and import on demand.

### 3.1 Model asset `data`

For `type: model` with `uri` (file-sourced), no `data` is required — the model
is imported from `uri`. For `type: model` with `primitive`, no `data` either —
the primitive is constructed from `primitive`. `data` is reserved for future
per-model override parameters.

### 3.2 Material asset `data` + `pipeline`

`data` maps 1:1 onto `MaterialData` (`src/Renderer/Material/Material.hpp`):

| Field | YAML type | Default |
|---|---|---|
| `color` | `[r,g,b]` (vec3) | `[1,1,1]` |
| `baseColorFactor` | `[r,g,b,a]` (vec4) | `[1,1,1,1]` |
| `diffuse` / `ambient` / `specular` / `emissive` | `[r,g,b]` | `[1,1,1]` / `[1,1,1]` / `[1,1,1]` / `[0,0,0]` |
| `shininess` | float | 0.0 |
| `opacity` | float | 1.0 |
| `metallicFactor` | float | 0.0 |
| `roughnessFactor` | float | 1.0 |
| `normalScale` | float | 1.0 |
| `occlusionStrength` | float | 1.0 |
| `specularStrength` | float | 1.0 |
| `reflectivity` | float | 0.0 |
| `emissiveIntensity` | float | 1.0 |
| `alphaMode` | `opaque` \| `mask` | `opaque` |
| `alphaCutoff` | float | 0.5 |
| `doubleSided` | bool | false |
| `textures` | list of `{type, path}` | — |

`textures` entries: `type` is one of `albedo|normal|metallic|roughness|ambientOcclusion|height|equirectangular`; `path` is the resolved source path.

`pipeline` maps 1:1 onto `MaterialPipelineConfig`:

| Field | YAML type | Default |
|---|---|---|
| `vertexShaderPath` | string | `shader.vert` |
| `fragmentShaderPath` | string | `shader.frag` |
| `enableAlphaBlending` | bool | false |
| `domain` | `opaque` \| `transparent` \| `water` | `opaque` |
| `tessControlShaderPath` | string | `""` (empty = standard v+f) |
| `tessEvalShaderPath` | string | `""` |

---

## 4. Component field definitions

Serialized field names are the **C++ data member names** (camelCase), matching
what a future reflection layer emits. Runtime-only members are omitted and
rehydrated by per-type hooks. Special types keep their member name but use a
per-type value handler (handles → asset GUID string, enums → lowercase string).

| Registered name | YAML fields | Notes |
|---|---|---|
| `Transform` | `translation: [x,y,z]`, `rotation: [x,y,z]` (radians), `scale: [x,y,z]` | defaults `[0,0,0]` / `[0,0,0]` / `[1,1,1]` |
| `RigidBody2D` | `velocity: [x,y]`, `mass: float` | mass default 1 |
| `Point Light` | `color: [r,g,b]`, `intensity: float`, `radius: float` | defaults `[1,1,1]` / 1 / 0.25 |
| `Directional Light` | `color: [r,g,b]`, `intensity: float` | direction derived from Transform |
| `Water` | `subdivisions: int` | default 64 |
| `Mesh` | `modelHandle: <assetId>`, `materialHandle: <assetId>`, `view: bool` | asset refs resolved via AssetDatabase |
| `Camera` | `primary: bool` | camera internals are runtime-only (move-only class) |
| `Post Process Stack` | `enabled: bool`, `effects: [{definitionId, enabled, parameters: {color: [r,g,b,a], params: [x,y,z,w]}}]` | nested + vector |
| `Lua Script` | `scriptPath: string` | sol2 state re-attached on load via `loadScript` |
| `Native Script` | `scriptPath: string`, `scriptName: string` | `IScript*` re-attached via `loadScript` |

Entity-level field: `name` (from `EntityMetadata`).

Scalar formats: vec2/3/4 are YAML flow lists of floats; bool/int/float/string are
YAML scalars; enums are lowercase strings.

---

## 5. Versioning & migration rules

- `schemaVersion` is a monotonic integer; v1 is the initial format. The loader
  supports 1.
- File version **<** supported → run the migration chain (none exist yet at v1;
  the framework is reserved for when they do).
- File version **>** supported → **refuse to load** with clear diagnostics
  (never silently corrupt).
- Within-version forward compatibility is provided by the warn-and-skip rules
  in §2.2 (unknown component types, unknown fields).

---

## 6. Worked example

See `assets/scenes/schema_example.faye` — a full valid scene containing the
editor camera, post-processing, primitives with materials, a file-sourced model,
and both script component types. It satisfies the loader hard requirements
(exactly one primary camera + a post-process entity).
