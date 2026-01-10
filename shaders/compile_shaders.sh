#!/bin/bash

# Script to compile GLSL shaders to SPIR-V headers
# Requires glslangValidator (from glslang package)

cd "$(dirname "$0")"

if ! command -v glslangValidator &> /dev/null; then
    echo "glslangValidator not found. Please install glslang."
    echo "On macOS: brew install glslang"
    exit 1
fi

# Compile vertex shader
echo "Compiling vertex shader..."
glslangValidator -V shader.vert -o vert.spv

# Compile fragment shader
echo "Compiling fragment shader..."
glslangValidator -V shader.frag -o frag.spv

# Convert to C header files
echo "Converting to header files..."
xxd -i vert.spv > vert.spv.h
xxd -i frag.spv > frag.spv.h

# Clean up intermediate files
rm vert.spv frag.spv

echo "Shader compilation complete!"
echo "Generated: vert.spv.h and frag.spv.h"
