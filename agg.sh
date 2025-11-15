#!/usr/bin/env bash

# Aggregate all .h and .cpp files from include/ and src/ into a single output file
# Usage: ./aggregate.sh output.txt

OUTFILE="$1"

if [ -z "$OUTFILE" ]; then
  echo "Usage: $0 <output-file>"
  exit 1
fi

# Empty or create the output file
: > "$OUTFILE"

# Function to append files from a directory
append_files() {
  local DIR="$1"
  local EXT="$2"

  for FILE in "$DIR"/*"$EXT"; do
    [ -e "$FILE" ] || continue
    echo "////" >> "$OUTFILE"
    echo "//// $(basename "$FILE")" >> "$OUTFILE"
    echo "////" >> "$OUTFILE"
    cat "$FILE" >> "$OUTFILE"
    echo -e "\n" >> "$OUTFILE"
  done
}

append_files include .h
append_files src .cpp

echo "Aggregated files into $OUTFILE"
