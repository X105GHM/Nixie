#!/usr/bin/env python3
"""Build and run the production time-series algorithm as a native host test."""

from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
COMPILER = shutil.which("g++")


def main() -> None:
    if not COMPILER:
        raise SystemExit("g++ is required for the native time-series unit tests")

    with tempfile.TemporaryDirectory(prefix="nixie-history-test-") as temporary:
        executable = Path(temporary) / "time_series_history_test.exe"
        command = [
            COMPILER,
            "-std=c++17",
            "-O2",
            "-pthread",
            "-Wall",
            "-Wextra",
            "-Werror",
            f"-I{ROOT / 'src'}",
            str(ROOT / "test/time_series_history_test.cpp"),
            str(ROOT / "src/History/TimeSeriesPyramid.cpp"),
            "-o",
            str(executable),
        ]
        subprocess.run(command, check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
