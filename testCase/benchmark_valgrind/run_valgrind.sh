#!/bin/bash

# Valgrind memory monitoring script
# Specifically designed to monitor memory allocation size for each function

EXECUTABLE="./benchmark_valgrind"
OUTPUT_DIR="memory_reports"

# Create output directory
mkdir -p $OUTPUT_DIR

echo "Starting function-level memory allocation monitoring..."

# Use Callgrind to monitor function calls and memory allocation relationship
echo "Running function-level memory monitoring..."
valgrind --tool=callgrind \
    --callgrind-out-file=$OUTPUT_DIR/function_memory.out \
    --collect-jumps=no \
    --simulate-cache=no \
    --dump-instr=yes \
    --collect-systime=yes \
    $EXECUTABLE

# Generate function-level analysis report
if command -v callgrind_annotate &> /dev/null; then
    callgrind_annotate --auto=yes --inclusive=yes \
        $OUTPUT_DIR/function_memory.out > $OUTPUT_DIR/function_memory_report.txt
    echo "Function memory report generated: $OUTPUT_DIR/function_memory_report.txt"
fi

# Extract and analyze key information
echo ""
echo "===== Function-level memory monitoring completed ====="
echo "Report file located at: $OUTPUT_DIR/"
echo ""
echo "View function memory analysis report:"
echo "- Function-level analysis: cat $OUTPUT_DIR/function_memory_report.txt"
echo ""
echo "=========================="
