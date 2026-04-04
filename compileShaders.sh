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

find src/shaders -type f \( -name '*.vert' -o -name '*.frag' -o -name '*.comp' \) | while IFS= read -r shader; do
	relative_path="${shader#src/shaders/}"
	output_path="src/shaders/compiled/${relative_path}.spv"
	mkdir -p "$(dirname "$output_path")"
	"${GLSLC}" "$shader" -o "$output_path"
done