"""Compares C++ and MATLAB pattern files byte for byte.

Usage: python compare.py <matlab out dir> <c++ out dir>
Exit code 0 only if every case matches exactly.
"""

import os
import sys


def main():
    ref_dir, cpp_dir = sys.argv[1], sys.argv[2]
    names = sorted(n for n in os.listdir(ref_dir) if n.endswith(".bin"))
    failures = 0
    total_bytes = 0

    for name in names:
        ref = open(os.path.join(ref_dir, name), "rb").read()
        cpp_path = os.path.join(cpp_dir, name)
        if not os.path.exists(cpp_path):
            print(f"MISSING  {name}")
            failures += 1
            continue
        cpp = open(cpp_path, "rb").read()
        total_bytes += len(ref)

        if ref == cpp:
            continue

        failures += 1
        if len(ref) != len(cpp):
            print(f"SIZE     {name}: matlab {len(ref)} bytes, c++ {len(cpp)} bytes")
            continue
        diffs = [i for i in range(len(ref)) if ref[i] != cpp[i]]
        worst = max(abs(ref[i] - cpp[i]) for i in diffs)
        first = diffs[0]
        print(f"DIFF     {name}: {len(diffs)} of {len(ref)} bytes differ (max {worst} uA); "
              f"first at sample {first // 64}, slot {first % 64}: matlab {ref[first]}, c++ {cpp[first]}")

    print(f"{len(names) - failures} of {len(names)} cases identical ({total_bytes:,} bytes compared)")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
