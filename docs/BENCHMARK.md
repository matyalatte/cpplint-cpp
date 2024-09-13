# Benchmarking

This repository contains some scripts for benchmarking.

## Execution time

[`benchmark.py`](../benchmark/benchmark.py) can measure the average execution time for cpplint-cpp and cpplint.py. It takes about two minutes for measurement.

```console
$ python ./benchmark/benchmark.py . --cpplint_cpp="./build/cpplint-cpp"
Measuring time for cpplint-cpp: ./build/cpplint-cpp --recursive --quiet --counting=detailed .
Measuring time for cpplint.py: python cpplint.py --recursive --quiet --counting=detailed .
Execution time for cpplint-cpp: x.xxxxxx seconds
Execution time for cpplint.py: x.xxxxxx seconds
```

## Memory usage

[`memory_usage.sh`](../benchmark/memory_usage.sh) can measure memory usage for a linter against a directory.

```console
$ sh ./benchmark/memory_usage.sh "./build/cpplint-cpp" .
Measuring memory usage: ./build/cpplint-cpp
Maximum memory usage: xx.xx MiB

$ sh ./benchmark/memory_usage.sh "python cpplint.py" .
Measuring memory usage: python cpplint.py
Maximum memory usage: xx.xx MiB
```

## Github Actions

You don't need to setup environment for benchmarking.
You can use [`benchmark.yml`](../.github/workflows/benchmark.yml) in your fork to run `benchmark.py` and `memory_usage.sh`.  

![benchmark.yml](https://github.com/user-attachments/assets/491f4c4b-32c4-4362-9406-2b1816e51574)
