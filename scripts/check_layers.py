#!/usr/bin/env python3
"""Enforce the engine/editor layer boundaries (Phase 1.8).

The target architecture separates the engine from the editor so a packaged
game can link the engine without ImGui/editor code. This script keeps the
inversions from creeping back in while the remaining known ones are resolved.

The engine's own modules (Assets, Scene, Runtime, Scripting) live under
src/engine/; the checker normalizes `engine/<Module>/...` back to its module
name so the rules are identical regardless of nesting.

Usage:
    python3 scripts/check_layers.py            # hard rules only
    python3 scripts/check_layers.py --strict   # also fail on known inversions
    python3 scripts/check_layers.py --quiet    # suppress the progress report

Layers (top-level under src/, or src/engine/<Module>/):
    Core, Platform, Assets, Scene, Renderer, Scripting, Runtime, Editor, ...

Hard rules (exit != 0 on violation):
  H1. Nothing outside src/Editor/ (and the editor PCH) may include Editor/
      headers or imgui.h. This is THE invariant that makes the engine buildable
      without the editor.
  H2. src/Core/ may not include Assets/, Platform/, Scene/, Renderer/,
      Scripting/, Runtime/, Editor/ -- except the documented allow-list below
      (SemanticRegistry.cpp is the one deliberate Core->Scene/Serialization
      bridge; the Core test target mirrors it).
  H3. src/Platform/ may not include Scene/, Renderer/ or Editor/ (raw input is
      scene-free; the editor camera lives in Editor/Utility/).
  H4. src/engine/Scripting/ may not include Editor/ or Renderer/.
  H5. src/Renderer/, src/engine/Scene/, src/engine/Assets/, src/engine/Runtime/
      may not include Editor/ or imgui (covered by H1).

Known inversions (reported as progress, failing only with --strict):
  I1. Assets/ModelRegistry -> Renderer/Resources/Model  (importer extraction, 1.5)
  I2. Scene/ -> Renderer/ headers                        (SceneManager, serialization)
  I3. Renderer/ -> Scene/ headers                        (extractors, RenderScene)

Run from the repository root.
"""

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
SRC = REPO_ROOT / "src"
INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"')

# Top-level layers under src/ (or under src/engine/) that we reason about.
LAYERS = [
    "Assets", "Core", "Editor", "Platform", "Renderer", "Runtime",
    "Scene", "Scripting",
]

# Documented Core exceptions: files under Core/ that are allowed to reach
# across the layer boundary. Keep this list as short as possible.
CORE_EXCEPTIONS = {
    "Core/ECS/Reflection/SemanticRegistry.cpp",  # the one Core->Scene/Serialization bridge
    "Core/ECS/Tests/ReflectionTests.cpp",        # test target mirrors the bridge
}

# H1 exceptions: files outside Editor/ that are allowed to reach the editor.
# engine/Runtime/main.cpp is the editor entry point (owns Editor::Application);
# Phase 2 (engine/editor build split) relocates it into the editor executable,
# after which this exception can be deleted.
H1_EXCEPTIONS = {"engine/Runtime/main.cpp"}

# Layers Core is not allowed to include (anything above the foundation).
CORE_BANNED_LAYERS = ["Assets", "Platform", "Scene", "Renderer",
                      "Scripting", "Runtime", "Editor"]

# Platform must stay scene-free (editor camera moved to Editor).
PLATFORM_BANNED_LAYERS = ["Scene", "Renderer", "Editor"]

SCRIPTING_BANNED_LAYERS = ["Editor", "Renderer"]


def module_layer(path_parts) -> str | None:
    """Map the leading path components to a layer name, or None."""
    if len(path_parts) < 1:
        return None
    # src/engine/<Module>/... -> <Module> (engine is a container, not a layer)
    if path_parts[0] == "engine":
        if len(path_parts) < 2:
            return None
        layer = path_parts[1]
    else:
        layer = path_parts[0]
    return layer if layer in LAYERS else None


def file_layer(rel: str) -> str | None:
    """Return the layer a file under src/ belongs to, or None."""
    return module_layer(rel.split("/"))


def include_layer(inc: str) -> str | None:
    """Return the layer an #include target belongs to, or None."""
    return module_layer(inc.split("/"))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--strict", action="store_true",
                        help="fail on known (not-yet-fixed) inversions too")
    parser.add_argument("--quiet", action="store_true",
                        help="suppress the known-inversion progress report")
    args = parser.parse_args()

    hard_violations = []  # (file, include, rule)
    known_inversions = []  # (file, include, kind)

    files = sorted(SRC.rglob("*"))
    for path in files:
        if not path.is_file():
            continue
        if path.suffix not in (".hpp", ".h", ".cpp"):
            continue
        rel = path.relative_to(SRC).as_posix()
        if rel.startswith("include/"):
            continue  # vendored third-party headers
        layer = file_layer(rel)
        if layer is None:
            continue

        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            m = INCLUDE_RE.match(line)
            if not m:
                continue
            inc = m.group(1)
            inc_layer = include_layer(inc)
            if inc_layer is None:
                continue

            # H1: editor isolation (also catches imgui.h via layer-less check)
            if inc_layer == "Editor" or inc.startswith("imgui"):
                if layer not in ("Editor", "Precompiled") and rel not in H1_EXCEPTIONS:
                    hard_violations.append((rel, inc, "H1 (editor isolation)"))
                continue

            if layer == "Core":
                if inc_layer != "Core" and rel not in CORE_EXCEPTIONS:
                    if inc_layer in CORE_BANNED_LAYERS:
                        hard_violations.append((rel, inc, "H2 (core is foundation)"))
            elif layer == "Platform":
                if inc_layer in PLATFORM_BANNED_LAYERS:
                    hard_violations.append((rel, inc, "H3 (platform stays scene-free)"))
            elif layer == "Scripting":
                if inc_layer in SCRIPTING_BANNED_LAYERS:
                    hard_violations.append((rel, inc, "H4 (scripting is clean)"))

            # Known inversions (tracked progress toward Phase 2)
            if layer == "Assets" and inc_layer == "Renderer":
                known_inversions.append((rel, inc, "I1 assets->renderer"))
            if layer == "Scene" and inc_layer == "Renderer":
                known_inversions.append((rel, inc, "I2 scene->renderer"))
            if layer == "Renderer" and inc_layer == "Scene":
                known_inversions.append((rel, inc, "I3 renderer->scene"))

    ok = True
    if hard_violations:
        ok = False
        print("HARD LAYER VIOLATIONS (must be fixed):")
        for rel, inc, rule in hard_violations:
            print(f"  [{rule}] {rel} -> {inc}")
    elif not args.quiet:
        print("Hard layer rules: OK")

    if args.strict and known_inversions:
        ok = False
        print("KNOWN INVERSIONS (--strict treats these as failures):")
        for rel, inc, kind in known_inversions:
            print(f"  [{kind}] {rel} -> {inc}")
    elif not args.quiet:
        from collections import Counter
        counts = Counter(kind for _, _, kind in known_inversions)
        summary = ", ".join(f"{k}: {v}" for k, v in sorted(counts.items())) or "none"
        print(f"Known inversions (not failing): {summary}")
        for rel, inc, kind in sorted(known_inversions):
            print(f"  [{kind}] {rel} -> {inc}")

    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
