#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# pin_deps.sh — print exact GIT_TAG lines for the dependency commits currently
# checked out in a build directory.
#
#   ./tools/pin_deps.sh build-c26
#
# Why: six dependencies in cmake/FayeDependencies.cmake track moving branches
# (imgui/docking, quill/master, assimp/master, spirv_reflect/main, lua/master,
# sol2/main). On a bleeding-edge toolchain that means a build failure may be an
# unrelated upstream regression, and a green build does not reproduce later.
#
# Pin to the SHAs of a build you have actually seen pass. Release tags would be
# prettier, but the SHA is the thing you verified — and for yaml-cpp there is no
# fixed release to move to anyway.
# ---------------------------------------------------------------------------
set -uo pipefail

BUILD="${1:-build-c26}"
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS="$REPO/$BUILD/_deps"

if [ ! -d "$DEPS" ]; then
  echo "no such dir: $DEPS" >&2
  echo "usage: ./tools/pin_deps.sh <build-dir>   (e.g. build-c26)" >&2
  exit 1
fi

printf '# Pinned from %s on %s\n' "$BUILD" "$(date -u +%Y-%m-%dT%H:%MZ)"
printf '# Paste each GIT_TAG line into cmake/FayeDependencies.cmake.\n\n'

for name in imgui quill assimp spirv_reflect lua sol2 glm doctest yaml-cpp boost; do
  src="$DEPS/${name}-src"
  [ -d "$src" ] || continue
  sha="$(git -C "$src" rev-parse HEAD 2>/dev/null)" || continue
  desc="$(git -C "$src" describe --tags --always 2>/dev/null || echo '-')"
  # GIT_SHALLOW must be FALSE when GIT_TAG is a raw SHA: a shallow clone can
  # only fetch a named ref, so CMake cannot resolve an arbitrary commit.
  printf '  # %-14s currently %s (%s)\n' "$name" "$desc" "$sha"
  printf '  GIT_TAG        %s\n' "$sha"
  printf '  GIT_SHALLOW    FALSE\n\n'
done

cat <<'EOF'
# NOTE: switching GIT_TAG from a branch name to a raw SHA requires
# GIT_SHALLOW FALSE — shallow clones can only fetch named refs. Clones get
# slower; that is the price of reproducibility. If you would rather keep
# GIT_SHALLOW TRUE, pin to a release TAG instead of a SHA (check each
# project's releases page for one at or after the commit above).
EOF
