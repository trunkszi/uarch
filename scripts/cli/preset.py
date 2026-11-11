"""Resolve supported toolchains against the actual configure presets."""

import json
import os
import re
import warnings
from pathlib import Path

from .config import COMPILER_ALIASES, PROJECT_ROOT


def _strip_json_comments(text: str) -> str:
    pattern = r'("(?:[^"\\]|\\.)*")|//[^\n]*|/\*[\s\S]*?\*/'
    return re.sub(pattern, lambda m: m.group(1) if m.group(1) is not None else " ", text)


def _load_presets_data() -> dict:
    return json.loads(_strip_json_comments((PROJECT_ROOT / "CMakePresets.json").read_text()))


def load_available_presets() -> set[str]:
    return {p["name"] for p in _load_presets_data()["configurePresets"] if not p.get("hidden", False)}


def get_preset_cache_variable(name: str, key: str):
    presets = {p["name"]: p for p in _load_presets_data()["configurePresets"]}

    def walk(current, seen):
        if current in seen:
            raise ValueError("Cyclic preset inheritance")
        p = presets[current]
        if key in p.get("cacheVariables", {}):
            v = p["cacheVariables"][key]
            return v.get("value") if isinstance(v, dict) else v
        parents = p.get("inherits", [])
        for parent in [parents] if isinstance(parents, str) else parents:
            value = walk(parent, seen | {current})
            if value is not None:
                return value
        return None

    return walk(name, set())


def resolve_preset(debug=False, dev=False, compiler="clang", sanitizer="asan-ubsan", *, preset=None) -> str:
    if debug and dev:
        raise ValueError("--debug and --dev are mutually exclusive")
    if compiler not in COMPILER_ALIASES:
        raise ValueError(f"Unknown compiler {compiler!r}; choose from {', '.join(COMPILER_ALIASES)}")
    if preset is None:
        toolchain = COMPILER_ALIASES[compiler]
        if sanitizer != "asan-ubsan":
            raise ValueError("Only asan-ubsan is supported by the uarch presets")
        if dev and toolchain.endswith("-musl"):
            warnings.warn(
                "Musl sanitizer presets are unavailable; using the corresponding glibc compiler", stacklevel=2
            )
            toolchain = toolchain.replace("-musl", "-glibc")
        preset = f"{toolchain}-{'asan-ubsan' if dev else 'debug' if debug else 'release'}"
    available = load_available_presets()
    if preset not in available:
        raise ValueError(f"Unknown preset {preset!r}; available: {', '.join(sorted(available))}")
    return preset


def _expand_preset_value(value):
    """Expand preset env macros. The CLI forwards the compiler as a literal -D."""
    if not isinstance(value, str):
        return value
    home = os.environ.get("HOME") or str(Path.home())
    return value.replace("$penv{HOME}", home).replace("$env{HOME}", home)


def get_preset_compiler(name: str):
    compiler = get_preset_cache_variable(name, "CMAKE_CXX_COMPILER")
    if compiler:
        return _expand_preset_value(str(compiler).replace("${sourceDir}", str(PROJECT_ROOT)))
    presets = {p["name"]: p for p in _load_presets_data()["configurePresets"]}

    def toolchain(current, seen):
        if current in seen:
            raise ValueError("Cyclic preset inheritance")
        entry = presets[current]
        if entry.get("toolchainFile"):
            return entry["toolchainFile"]
        parents = entry.get("inherits", [])
        for parent in [parents] if isinstance(parents, str) else parents:
            found = toolchain(parent, seen | {current})
            if found:
                return found
        return None

    filename = (
        get_preset_cache_variable(name, "VCPKG_CHAINLOAD_TOOLCHAIN_FILE")
        or toolchain(name, set())
        or get_preset_cache_variable(name, "CMAKE_TOOLCHAIN_FILE")
    )
    if not filename:
        return None
    path = Path(_expand_preset_value(str(filename).replace("${sourceDir}", str(PROJECT_ROOT))))
    if not path.is_absolute():
        path = PROJECT_ROOT / path
    text = path.read_text()
    match = re.search(r'(?m)^\s*set\(\s*CMAKE_CXX_COMPILER\s+(?:"([^"\n]+)"|([^\s)]+))', text)
    if not match:
        return None
    value = (match.group(1) or match.group(2)).replace("${CMAKE_CURRENT_LIST_DIR}", str(path.parent))
    if "${_tc_root}" in value:
        roots = re.findall(r'(?m)^\s*set\(\s*_tc_root\s+"([^"\n]+)"', text)
        override = os.environ.get("TOOLCHAIN_ROOT")
        selected = next((v for v in roots if ("$ENV{TOOLCHAIN_ROOT}" in v) == bool(override)), None)
        if selected is None:
            return None
        selected = selected.replace("$ENV{TOOLCHAIN_ROOT}", override or "")
        value = value.replace("${_tc_root}", selected)
    return _expand_preset_value(value.replace("$ENV{HOME}", os.environ.get("HOME") or str(Path.home())))
