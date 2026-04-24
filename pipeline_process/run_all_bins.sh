#!/usr/bin/env bash
set -euo pipefail

if [ $# -lt 2 ]; then
    echo "Usage: $0 <metadata.csv> <input_folder> [runs]"
    echo "Example: $0 metadata.csv radar_bin_output_100samples 5"
    exit 1
fi

METADATA="$1"
INPUT_DIR="$2"
RUNS="${3:-5}"

if [ ! -f "$METADATA" ]; then
    echo "metadata file not found: $METADATA"
    exit 1
fi

if [ ! -d "$INPUT_DIR" ]; then
    echo "input folder not found: $INPUT_DIR"
    exit 1
fi

OUT_DIR="batch_out"
mkdir -p "$OUT_DIR"

shopt -s nullglob
for real in "$INPUT_DIR"/*_real.bin; do
    base=$(basename "$real" _real.bin)
    imag="$INPUT_DIR/${base}_imag.bin"

    if [ ! -f "$imag" ]; then
        echo "skip: $base (imag file missing)"
        continue
    fi

    echo "============================================================"
    echo "Running sample: $base"
    echo "  real = $real"
    echo "  imag = $imag"

    ./radarBench "$METADATA" "$real" "$imag"         "$OUT_DIR/${base}_timing.csv"         "$OUT_DIR/${base}_detections.csv"         "$RUNS" | tee "$OUT_DIR/${base}.log"
done
