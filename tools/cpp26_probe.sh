#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# cpp26_probe.sh — does this tree build under GCC 16 / C++26 / -freflection?
#
# Run this yourself; it was authored without the ability to execute it, so
# treat every step as a hypothesis until it prints OK.
#
#   ./tools/cpp26_probe.sh            # run every stage, stop at first failure
#   ./tools/cpp26_probe.sh 5          # run only stage 5
#   ./tools/cpp26_probe.sh --list     # show stages
#
# Creates ONLY: build-c26-baseline/ build-c26/ build-c26-pch/ build-c26-refl/
# Never touches build/ build-release/ build-tsan/ build-serial/ or any source.
#
# Stages are split so a failure is ATTRIBUTABLE. Stage 4 (GCC 16 @ C++20) vs
# stage 6 (GCC 16 @ C++26) vs stage 9 (+ -freflection) is the whole point: skip
# the split and you cannot tell a compiler regression from a standard
# incompatibility from an experimental-flag bug.
# ---------------------------------------------------------------------------
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO"
LOGDIR="$REPO/.probe-logs"; mkdir -p "$LOGDIR"
JOBS="$(nproc 2>/dev/null || echo 4)"

CXX=g++-16
CC=gcc-16

# --- Probe-only crutch ------------------------------------------------------
# GCC 15 stopped providing <cstdint> (and <algorithm>, <cstring>, ...)
# transitively through other libstdc++ headers. Any library predating that
# change breaks with "'uint16_t' was not declared in this scope". CONFIRMED
# here: yaml-cpp 0.8.0 emitterutils.cpp:221. This is a GCC-15 issue, NOT a
# C++26 issue and NOT your code.
#
# Force-including <cstdint> globally is harmless and lets ONE probe run surface
# every OTHER breakage instead of one per rebuild. The real fix is targeted and
# per-dependency — see docs/IMPLEMENTATION_PLAN.md Stage 0a.
# Override with: PROBE_CXX_FLAGS="" ./tools/cpp26_probe.sh
PROBE_CXX_FLAGS="${PROBE_CXX_FLAGS--include cstdint}"

# Keep building after the first error so one run collects ALL failures.
GEN_ARGS=(); KEEP=(-- -k)
if command -v ninja >/dev/null 2>&1; then GEN_ARGS=(-G Ninja); KEEP=(-- -k 0); fi

errsum() {  # errsum <logfile> — unique "file: error" lines, deduped by file
  grep -E 'error:' "$1" 2>/dev/null \
    | sed -E 's|^/[^ ]*/_deps/([^/]+)/.*|  [dep: \1] &|' \
    | awk '!seen[$0]++' | head -25
}

ok()   { printf '\033[32m  OK\033[0m   %s\n' "$*"; }
bad()  { printf '\033[31m FAIL\033[0m  %s\n' "$*"; }
warn() { printf '\033[33m WARN\033[0m  %s\n' "$*"; }
hdr()  { printf '\n\033[1m=== %s ===\033[0m\n' "$*"; }

stage_0_environment() {
  hdr "0 · Environment"
  echo "cmake: $(cmake --version 2>/dev/null | head -1)"
  local cmv; cmv=$(cmake --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)
  # CMake >= 3.30 REQUIRED, not 3.28. cxx_std_26 was documented in 3.25 but
  # only implemented in 3.30; on 3.28 every target_compile_features(cxx_std_26)
  # fails with "not known to CXX compiler" regardless of the compiler version.
  # Ubuntu 24.04 ships 3.28.
  if [ -n "$cmv" ] && awk -v v="$cmv" 'BEGIN{split(v,a,".");exit !(a[1]>3||(a[1]==3&&a[2]>=30))}'; then
    ok "cmake >= 3.30"
  else
    bad "cmake $cmv is too old — need >= 3.30 (cxx_std_26 was only implemented there)."
    warn "Upgrade via Kitware's APT repo https://apt.kitware.com"
    warn "or:  pip install --upgrade cmake"
    return 1
  fi
  echo "available compilers:"; ls /usr/bin/g++-* 2>/dev/null || echo "  (none versioned)"
  if command -v "$CXX" >/dev/null 2>&1; then
    ok "$CXX present: $($CXX --version | head -1)"
  else
    bad "$CXX not found."
    cat <<'EOF'

  To install on Ubuntu, in order of preference:
    1) sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
       sudo apt update && apt-cache policy g++-16
       # Only if a candidate is listed:
       sudo apt install -y g++-16
    2) If the PPA has no g++-16 yet, use a container:
       docker run --rm -it -v "$PWD":/src -w /src gcc:16 bash
    3) Last resort: build GCC trunk from source (1-2 hours).

  Do NOT substitute GCC 15 and assume the result transfers.
  -freflection does not exist there.
EOF
    return 1
  fi
}

stage_1_reflection_available() {
  hdr "1 · Does this compiler actually have reflection?"
  echo 'int main(){}' > /tmp/faye_p1.cpp
  if $CXX -std=c++26 -freflection /tmp/faye_p1.cpp -o /tmp/faye_p1 2>"$LOGDIR/flag.log"; then
    ok "-std=c++26 -freflection accepted"
  else
    bad "-freflection rejected — see $LOGDIR/flag.log"; return 1
  fi

  cat > /tmp/faye_p2.cpp <<'EOF'
#include <meta>
struct S { int a; float b; };
consteval int countMembers() {
    int n = 0;
    for (auto m : std::meta::nonstatic_data_members_of(
                      ^^S, std::meta::access_context::current())) { (void)m; ++n; }
    return n;
}
static_assert(countMembers() == 2);
int main(){}
EOF
  if $CXX -std=c++26 -freflection /tmp/faye_p2.cpp -o /tmp/faye_p2 2>"$LOGDIR/meta.log"; then
    ok "<meta> + nonstatic_data_members_of + access_context::current() work AS SPELLED"
  else
    warn "the exact API spelling in REFLECTION_DESIGN.md 3 is wrong for this compiler."
    warn "This is EXPECTED — P3795 is still moving these names. Read $LOGDIR/meta.log,"
    warn "find the real spellings in the GCC 16 <meta> header, and update the doc."
    echo "  header location guess:"; $CXX -std=c++26 -E -x c++ - <<< '#include <meta>' 2>/dev/null | grep -m1 'meta"' || true
  fi

  cat > /tmp/faye_p3.cpp <<'EOF'
#include <meta>
struct Range { float lo, hi; };
struct S { [[=Range{0.f,1.f}]] float x; };
int main(){}
EOF
  if $CXX -std=c++26 -freflection /tmp/faye_p3.cpp -o /tmp/faye_p3 2>"$LOGDIR/annot.log"; then
    ok "P3394 annotation syntax [[=Value]] accepted"
  else
    warn "annotations rejected — see $LOGDIR/annot.log. The entire 4 editor"
    warn "vocabulary depends on this. If it is genuinely absent, reflection is"
    warn "still useful but the annotation design must be rethought."
  fi
}

stage_2_pin_check() {
  hdr "2 · Are the dependencies pinned?"
  # An unpinned probe is not evidence: a re-clone of a moving branch means a
  # failure may be an unrelated upstream regression, and a success does not
  # reproduce next week.
  local f="cmake/FayeDependencies.cmake" moving=0
  while IFS= read -r line; do
    case "$line" in *GIT_TAG*)
      if echo "$line" | grep -qE 'GIT_TAG[[:space:]]+(master|main|docking|develop|trunk)[[:space:]]*$'; then
        warn "moving ref: $(echo "$line" | xargs)"; moving=1
      fi;;
    esac
  done < "$f"
  if [ "$moving" -eq 0 ]; then ok "all GIT_TAGs look pinned"
  else
    bad "Pin the refs above to release tags in $f BEFORE trusting any result below."
    warn "Continuing anyway, but treat the outcome as indicative only."
  fi
}

stage_4_baseline_cxx20() {
  hdr "4 · Baseline — GCC 16 at C++20 (isolates 'GCC 16 broke it')"
  [ -n "$PROBE_CXX_FLAGS" ] && warn "using probe crutch CXXFLAGS: $PROBE_CXX_FLAGS"
  cmake -S . -B build-c26-baseline "${GEN_ARGS[@]}" \
        -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX \
        -DCMAKE_BUILD_TYPE=Debug -DFAYE_USE_PCH=OFF \
        -DCMAKE_CXX_FLAGS="$PROBE_CXX_FLAGS" \
        > "$LOGDIR/04-configure.log" 2>&1 || { bad "configure failed — $LOGDIR/04-configure.log"; return 1; }
  # PCH OFF deliberately: FayePCH.hpp force-includes ~30 std headers into every
  # TU, which masks missing #includes engine-wide. libstdc++ prunes internal
  # includes every major release, so GCC 16 will surface them. Find them all at
  # once rather than one target at a time.
  if cmake --build build-c26-baseline -j"$JOBS" "${KEEP[@]}" > "$LOGDIR/04-build.log" 2>&1; then
    ok "tree builds with GCC 16 at C++20"
  else
    bad "GCC 16 breaks the tree even at C++20 — $LOGDIR/04-build.log"
    warn "Everything that failed, in one pass (keep-going was on):"
    errsum "$LOGDIR/04-build.log"
    warn "'was not declared in this scope' on a fixed-width int type = the GCC 15"
    warn "transitive-include change. Fix per-dep with -include cstdint, not by"
    warn "abandoning the migration. See IMPLEMENTATION_PLAN.md Stage 0a."
    return 1
  fi
}

stage_5_sol2_verdict() {
  hdr "5 · sol2 under C++26 — the cheapest decision gate in the whole plan"
  local SOL LUAINC
  SOL="$(find build-c26-baseline/_deps -maxdepth 3 -type d -name include -path '*sol2*' 2>/dev/null | head -1)"
  LUAINC="$(find build-c26-baseline/_deps -maxdepth 5 -type d -name include -path '*lua*' 2>/dev/null | head -1)"
  if [ -z "$SOL" ]; then warn "sol2 sources not found — skipping (stage 4 may not have run)"; return 0; fi
  printf '#include <sol/sol.hpp>\nint main(){ sol::state s; s.open_libraries(sol::lib::base); return 0; }\n' > /tmp/faye_sol.cpp
  if $CXX -std=c++26 -fsyntax-only -I"$SOL" ${LUAINC:+-I"$LUAINC"} /tmp/faye_sol.cpp 2>"$LOGDIR/05-sol2.log"; then
    ok "sol2 SURVIVES C++26 — Lua removal is optional, not forced"
  else
    bad "sol2 IS DEAD under C++26 — $LOGDIR/05-sol2.log"
    warn "Execute the Lua removal (IMPLEMENTATION_PLAN.md Stage 0b) on a branch,"
    warn "then re-run this script from stage 6 against a tree with no sol2 in it."
  fi
}

stage_6_deps_at_cxx26() {
  hdr "6 · Dependencies at C++26, no reflection"
  cmake -S . -B build-c26 "${GEN_ARGS[@]}" \
        -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX \
        -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_STANDARD=26 -DFAYE_USE_PCH=OFF \
        -DCMAKE_CXX_FLAGS="$PROBE_CXX_FLAGS" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        > "$LOGDIR/06-configure.log" 2>&1 || {
      bad "configure failed — $LOGDIR/06-configure.log"
      warn "If this is FetchContent_Populate(boost): the single-argument form is"
      warn "an error under CMP0169. See IMPLEMENTATION_PLAN.md Stage 0a."
      return 1; }
  # Deliberately does NOT stop at the first failing dep — build them all, so one
  # run tells you the complete list of what C++26 breaks.
  local rc=0
  for t in assimp quill yaml-cpp spirv-reflect-static lua_static faye_imgui; do
    if cmake --build build-c26 -j"$JOBS" --target "$t" "${KEEP[@]}" > "$LOGDIR/06-$t.log" 2>&1
      then ok "$t"; else bad "$t — $LOGDIR/06-$t.log"; errsum "$LOGDIR/06-$t.log"; rc=1; fi
  done
  [ $rc -ne 0 ] && warn "Above is the COMPLETE dep breakage list at C++26. Triage once, not iteratively."
  return $rc
}

stage_7_engine_targets() {
  hdr "7 · Engine targets, smallest first"
  local rc=0
  # faye_rotator_script first: it is the only target with no PCH and no
  # target_link_libraries, so it is the missing-include canary.
  for t in faye_rotator_script faye_tests faye_app; do
    if cmake --build build-c26 -j"$JOBS" --target "$t" "${KEEP[@]}" > "$LOGDIR/07-$t.log" 2>&1
      then ok "$t"; else bad "$t — $LOGDIR/07-$t.log"; errsum "$LOGDIR/07-$t.log"; rc=1; fi
  done
  ctest --test-dir build-c26 --output-on-failure > "$LOGDIR/07-ctest.log" 2>&1 \
    && ok "ctest green" || warn "ctest failures — $LOGDIR/07-ctest.log"
  return $rc
}

stage_8_pch_on() {
  hdr "8 · Re-enable the PCH (separate variable — attribute failures correctly)"
  cmake -S . -B build-c26-pch "${GEN_ARGS[@]}" \
        -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX \
        -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_STANDARD=26 -DFAYE_USE_PCH=ON \
        -DCMAKE_CXX_FLAGS="$PROBE_CXX_FLAGS" \
        > "$LOGDIR/08-configure.log" 2>&1 || { bad "configure failed"; return 1; }
  cmake --build build-c26-pch -j"$JOBS" "${KEEP[@]}" > "$LOGDIR/08-build.log" 2>&1 \
    && ok "PCH + C++26 fine" || { bad "PCH breaks at C++26 — $LOGDIR/08-build.log"; errsum "$LOGDIR/08-build.log"; return 1; }
}

stage_9_reflection_flag() {
  hdr "9 · -freflection on ENGINE translation units only"
  # ---------------------------------------------------------------------------
  # TWO earlier attempts at this stage failed, and both failures were
  # informative rather than fatal:
  #
  #   run 2: -freflection via global CMAKE_CXX_FLAGS broke CMake's own
  #          project() compiler check, which runs at the compiler's DEFAULT
  #          standard (gnu++23), not CMAKE_CXX_STANDARD.
  #   run 3: adding -std=c++26 to CMAKE_CXX_FLAGS fixed configure, then broke
  #          the BUILD -- because glm and assimp pin their OWN CXX_STANDARD
  #          (17), and CMake appends the target's -std AFTER CMAKE_CXX_FLAGS.
  #          So those TUs compiled at c++17 with -freflection still attached,
  #          and cc1plus rejects that combination.
  #
  # Conclusion, now proven twice over: -freflection CANNOT be a global flag.
  # It belongs on the C++26 engine targets and nowhere else. That is exactly
  # what IMPLEMENTATION_PLAN.md Stage 0a specifies via target_compile_options.
  #
  # So this stage no longer tries to force a global build. It replays the REAL
  # compile command for engine TUs with -freflection added -- which is the
  # actual question ("does reflection coexist with our headers?") -- with zero
  # repo edits and zero dependency rebuilds.
  # ---------------------------------------------------------------------------
  local CCJ="build-c26/compile_commands.json"
  [ -f "$CCJ" ] || { bad "no $CCJ — run stage 6 first (it now exports them)"; return 1; }

  # Emits TWO files:
  #   faye_refl_cmds.sh  — replay N engine TUs with -freflection
  #   faye_refl_base.sh  — one engine TU's command with the SOURCE FILE removed,
  #                        so our own test file can be dropped in and inherit
  #                        every include path and define verbatim. Harvesting
  #                        -I/-isystem by hand got this wrong: paths in
  #                        compile_commands.json are relative to "directory",
  #                        so they must be run from there, not from the repo root.
  python3 - "$CCJ" 2>"$LOGDIR/09-extract.log" <<'PY'
import json, sys, shlex, os
db = json.load(open(sys.argv[1]))
prefer = ("Components.cpp", "RegisterComponents.cpp", "ComponentSerializers.cpp",
          "Scene.cpp", "Engine.cpp")
eng = [e for e in db if "/_deps/" not in e["file"] and e["file"].endswith(".cpp")]
if not eng:
    sys.stderr.write("no engine entries in compile_commands.json\n"); sys.exit(1)
picked = [e for e in eng if any(e["file"].endswith(p) for p in prefer)] or eng[:5]

def strip(e, drop_source):
    argv = shlex.split(e.get("command") or " ".join(e["arguments"]))
    base = os.path.basename(e["file"])
    out, skip = [], False
    for a in argv:
        if skip:                    skip = False; continue
        if a == "-o":               skip = True;  continue
        if a in ("-c",):            continue
        if a.startswith("-std="):   continue
        if drop_source and (a == e["file"] or os.path.basename(a) == base): continue
        out.append(a)
    out += ["-std=c++26", "-freflection", "-fsyntax-only"]
    return out

with open("/tmp/faye_refl_cmds.sh", "w") as f:
    for e in picked[:5]:
        f.write("cd %s && %s\n" % (shlex.quote(e["directory"]),
                " ".join(shlex.quote(t) for t in strip(e, False))))

e = picked[0]
with open("/tmp/faye_refl_base.sh", "w") as f:
    f.write("cd %s && %s" % (shlex.quote(e["directory"]),
            " ".join(shlex.quote(t) for t in strip(e, True))))
PY
  [ -s /tmp/faye_refl_cmds.sh ] || { bad "could not extract engine compile commands — $LOGDIR/09-extract.log"; cat "$LOGDIR/09-extract.log"; return 1; }

  local rc=0 n=0
  while IFS= read -r cmd; do
    n=$((n+1))
    if eval "$cmd" >> "$LOGDIR/09-tu.log" 2>&1; then ok "TU $n compiles with -freflection"
    else bad "TU $n FAILS with -freflection"; rc=1; fi
  done < /tmp/faye_refl_cmds.sh
  [ $rc -ne 0 ] && { errsum "$LOGDIR/09-tu.log"; return 1; }

  # Now the real question: can we reflect over an ACTUAL engine component,
  # through the engine's own headers and include paths?
  cat > /tmp/faye_refl_component.cpp <<'EOF'
#include "Scene/Entities/Components.hpp"
#include <meta>
#include <string_view>

// NOTE: '^^' must name the entity DIRECTLY. A using-declaration
// ("using Faye::TransformComponent;" then "^^TransformComponent") is rejected:
//     error: '^^' cannot be applied to a using-declaration
// Use the qualified name. This matters for the real implementation only where
// someone reflects a bare alias -- describe<T>() using '^^T' is unaffected.
consteval size_t fieldCount() {
    size_t n = 0;
    for (auto m : std::meta::nonstatic_data_members_of(
                      ^^Faye::TransformComponent, std::meta::access_context::current()))
        { (void)m; ++n; }
    return n;
}
// The name comparison stays INSIDE one consteval call: identifier_of's
// string_view is not guaranteed to survive escaping constant evaluation
// (which is why P2996 provides define_static_string).
consteval bool firstFieldIsTranslation() {
    for (auto m : std::meta::nonstatic_data_members_of(
                      ^^Faye::TransformComponent, std::meta::access_context::current()))
        return std::meta::identifier_of(m) == std::string_view{"translation"};
    return false;
}
static_assert(fieldCount() >= 3, "expected translation/rotation/scale");
static_assert(firstFieldIsTranslation(), "field names not readable by reflection");
int main() {}
EOF
  if eval "$(cat /tmp/faye_refl_base.sh) /tmp/faye_refl_component.cpp" \
       > "$LOGDIR/09-component.log" 2>&1; then
    ok "REFLECTED A REAL ENGINE COMPONENT — TransformComponent fields readable by name"
    ok "GREEN LIGHT: the reflection design is viable on this toolchain."
  else
    bad "reflecting TransformComponent failed — $LOGDIR/09-component.log"
    warn "If this is identifier_of/access_context, adjust REFLECTION_DESIGN.md 3 spellings."
    errsum "$LOGDIR/09-component.log"; return 1
  fi
}

STAGES=(stage_0_environment stage_1_reflection_available stage_2_pin_check
        stage_4_baseline_cxx20 stage_5_sol2_verdict stage_6_deps_at_cxx26
        stage_7_engine_targets stage_8_pch_on stage_9_reflection_flag)

if [ "${1:-}" = "--list" ]; then printf '%s\n' "${STAGES[@]}"; exit 0; fi

if [ -n "${1:-}" ]; then
  for s in "${STAGES[@]}"; do [[ "$s" == stage_${1}_* ]] && { "$s"; exit $?; }; done
  echo "no such stage: $1"; exit 2
fi

for s in "${STAGES[@]}"; do
  "$s" || { printf '\n\033[31mStopped at %s. Logs in %s\033[0m\n' "$s" "$LOGDIR"; exit 1; }
done
printf '\n\033[32mAll stages passed. Logs in %s\033[0m\n' "$LOGDIR"
