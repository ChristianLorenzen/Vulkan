#!/usr/bin/env sh

set -eu

if command -v glslc >/dev/null 2>&1; then
	GLSLC=glslc
elif [ -n "${VULKAN_SDK:-}" ] && [ -x "${VULKAN_SDK}/bin/glslc" ]; then
	GLSLC="${VULKAN_SDK}/bin/glslc"
else
	echo "glslc not found. Install shaderc/glslc or set VULKAN_SDK." >&2
	exit 1
fi

"${GLSLC}" src/shaders/shader.vert -o src/shaders/vert.spv
"${GLSLC}" src/shaders/shader.frag -o src/shaders/frag.spv