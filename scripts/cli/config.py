"""Project paths and CMake component contracts."""

from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_COMPILER = "clang"
COMPILER_ALIASES = {
    "clang": "clang-glibc",
    "gcc": "gcc-glibc",
    "musl-clang": "clang-musl",
    "musl-gcc": "gcc-musl",
    "clang-glibc": "clang-glibc",
    "gcc-glibc": "gcc-glibc",
    "clang-musl": "clang-musl",
    "gcc-musl": "gcc-musl",
}
COMPONENTS = {"tests": "UARCH_BUILD_TESTS", "bench": "UARCH_BUILD_BENCHES", "tools": "UARCH_BUILD_TOOLS"}


def get_build_directory(preset: str) -> Path:
    return PROJECT_ROOT / "build" / preset
