import argparse
import os
import subprocess
import time

def measure_time(command, repeat_time=30):
    duration = 0
    count = 0
    while(duration < repeat_time):
        start_time = time.time()  # Record start time
        result = subprocess.run(command, shell=True)  # Execute the command
        end_time = time.time()  # Record end time
        duration += end_time - start_time
        count += 1
    return duration / count


def get_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("file", help="path to source codes")
    parser.add_argument("--cpplint_cpp", default="./build/cpplint-cpp", type=str,
                        help="path to cpplint-cpp")
    parser.add_argument("--cpplint_py", default="python cpplint.py", type=str,
                        help="command to run cpplint.py")
    parser.add_argument("--options", default="--recursive --quiet --counting=detailed", type=str,
                        help="options for cpplint")
    parser.add_argument("--time", default=30, type=int,
                        help="Minimum measurement time for a command. Default to 30 (sec)")
    return parser.parse_args()


if __name__ == '__main__':
    args = get_args()
    file = args.file
    options = args.options
    repeat_time = args.time

    # Commands to compare
    cmd_cpp = f"{args.cpplint_cpp} {options} {file}"
    cmd_py = f"{args.cpplint_py} {options} {file}"

    if os.name == 'nt':
        # Fix paths for Windows
        cmd_cpp = cmd_cpp.replace("/", "\\")
        cmd_py = cmd_py.replace("/", "\\")

    # Measure time for cpplint-cpp
    print(f"Measuring time for cpplint-cpp: {cmd_cpp}")
    time1 = measure_time(cmd_cpp, repeat_time)

    # Measure time for cpplint-py
    print(f"Measuring time for cpplint.py: {cmd_py}")
    time2 = measure_time(cmd_py, repeat_time)

    # Output result
    print(f"Execution time for cpplint-cpp: {time1:.6f} seconds")
    print(f"Execution time for cpplint-py: {time2:.6f} seconds")
