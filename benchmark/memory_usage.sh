#!/bin/sh
# Script to measure memory usage with cpplint
#
# Examples
#  sh ./benchmark/memory_usage.sh "python3 cpplint.py" .
#  sh ./benchmark/memory_usage.sh "./build/cpplint-cpp" ../googletest-1.4.0

echo Measuring memory usage: $1
kib="$(/usr/bin/time -f "%M" sh -c "$1 --recursive --quiet --counting=detailed $2 >/dev/null 2>&1 || exit 0" 2>&1)"
mib=$(echo "scale=2; $kib / 1024" | bc)
echo Maximum memory usage: ${mib} MiB
