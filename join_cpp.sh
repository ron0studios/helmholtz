#!/usr/bin/env bash

# Usage: ./join_cpp.sh <input_folder> <output_file>

INPUT_DIR="$1"
OUTPUT_FILE="$2"

if [ -z "$INPUT_DIR" ] || [ -z "$OUTPUT_FILE" ]; then
    echo "Usage: $0 <input_folder> <output_file>"
    exit 1
fi

# Empty the output file if it exists
> "$OUTPUT_FILE"

for file in "$INPUT_DIR"/*.cpp; do
    if [ -f "$file" ]; then
        basename=$(basename "$file")
        echo "///" >> "$OUTPUT_FILE"
        echo "/// $basename" >> "$OUTPUT_FILE"
        echo "///" >> "$OUTPUT_FILE"
        cat "$file" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
    fi
done
